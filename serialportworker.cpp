#include "serialportworker.h"

#include <QDebug>
#include <QMutexLocker>
#include <QTimerEvent>
#include <algorithm>

SerialPortWorker::SerialPortWorker(QObject *parent)
    : QObject(parent)
    , m_serial(nullptr)
    , m_buffer(std::make_unique<RingBuffer>(1024 * 1024))
    , m_watchdogTimer(new QTimer(this))
    , m_processTimer(new QTimer(this))
{
    m_watchdogTimer->setInterval(500);
    m_processTimer->setInterval(kProcessIntervalMs);
    m_processTimer->setSingleShot(false);
}

void SerialPortWorker::initializeSerialPort() {
    if (m_serial) {
        disconnect(m_serial.data(), &QSerialPort::readyRead,
                   this, &SerialPortWorker::onDataReceived);
        disconnect(m_serial.data(), &QSerialPort::errorOccurred,
                   this, &SerialPortWorker::handleError);
    }

    m_serial.reset(new QSerialPort);

    connect(m_serial.data(), &QSerialPort::readyRead,
            this, &SerialPortWorker::onDataReceived, Qt::DirectConnection);
    connect(m_serial.data(), &QSerialPort::errorOccurred,
            this, &SerialPortWorker::handleError, Qt::DirectConnection);

    connect(m_watchdogTimer, &QTimer::timeout, this, &SerialPortWorker::watchdogTimeout);
    connect(m_processTimer, &QTimer::timeout, this, &SerialPortWorker::processPackets);
}

void SerialPortWorker::startPort(const SerialSettings &settings) {
    QMutexLocker lock(&m_mutex);

    if (!m_serial) {
        emit fatalError(QStringLiteral("Serial port not initialized"));
        return;
    }

    m_processTimer->stop();
    m_watchdogTimer->stop();

    if (m_serial->isOpen()) {
        m_serial->close();
    }

    m_serial->setPortName(settings.portName);
    m_serial->setBaudRate(settings.baudRate);
    m_serial->setDataBits(settings.dataBits);
    m_serial->setParity(settings.parity);
    m_serial->setStopBits(settings.stopBits);
    m_serial->setFlowControl(settings.flowControl);
    m_serial->setReadBufferSize(1024 * 1024);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        const QString err = m_serial->errorString();
        emit errorOccurred(err.isEmpty() ? QStringLiteral("Failed to open serial port") : err);
        return;
    }

    m_lastSettings = settings;
    m_hasSettings = true;

    QThread::currentThread()->setPriority(QThread::TimeCriticalPriority);

    m_lastActiveTime = QDateTime::currentMSecsSinceEpoch();
    m_seenActivity = false;
    m_watchdogTimer->start();
    m_processTimer->start();
    m_buffer->clear();
    emit portOpened();
}

void SerialPortWorker::stopPort() {
    QMutexLocker lock(&m_mutex);
    m_watchdogTimer->stop();
    m_processTimer->stop();
    m_buffer->clear();
    if (m_serial && m_serial->isOpen()) {
        m_serial->close();
    }
    emit portClosed();
}

void SerialPortWorker::restartPort() {
    SerialSettings settings;
    {
        QMutexLocker lock(&m_mutex);
        if (m_hasSettings) {
            settings = m_lastSettings;
        } else if (m_serial && m_serial->isOpen()) {
            settings.portName = m_serial->portName();
            settings.baudRate = static_cast<QSerialPort::BaudRate>(m_serial->baudRate());
            settings.dataBits = m_serial->dataBits();
            settings.parity = m_serial->parity();
            settings.stopBits = m_serial->stopBits();
            settings.flowControl = m_serial->flowControl();
            m_lastSettings = settings;
            m_hasSettings = true;
        } else {
            emit errorOccurred(QStringLiteral("No saved serial settings for restart"));
            return;
        }
    }

    stopPort();

    QTimer::singleShot(500, this, [this, settings]() {
        startPort(settings);
    });
}

void SerialPortWorker::writeToPort(const QByteArray &data) {
    QMutexLocker lock(&m_mutex);
    if (!m_serial || !m_serial->isOpen()) {
        emit infoMessage(QStringLiteral("writeToPort skipped: serial port not open"));
        return;
    }

    const qint64 bytesWritten = m_serial->write(data);
    if (bytesWritten == -1) {
        emit errorOccurred(QStringLiteral("Failed to write data: %1").arg(m_serial->errorString()));
    } else {
        m_seenActivity = true;
        m_lastActiveTime = QDateTime::currentMSecsSinceEpoch();
    }
}

void SerialPortWorker::onDataReceived() {
    QSerialPort *port = nullptr;
    {
        QMutexLocker lock(&m_mutex);
        port = m_serial.data();
        if (!port || !port->isOpen()) {
            emit infoMessage(QStringLiteral("onDataReceived skipped: serial port not open"));
            return;
        }
    }

    QByteArray data;
    while (true) {
        QByteArray chunk;
        {
            QMutexLocker lock(&m_mutex);
            if (!m_serial || !m_serial->isOpen()) {
                break;
            }
            if (m_serial->bytesAvailable() <= 0) {
                break;
            }
            chunk = m_serial->read(m_serial->bytesAvailable());
        }

        if (chunk.isEmpty()) {
            break;
        }
        data.append(chunk);
    }

    if (data.isEmpty()) {
        return;
    }

    m_lastActiveTime = QDateTime::currentMSecsSinceEpoch();
    m_seenActivity = true;

    if (!m_buffer->write(data)) {
        const size_t dropLen = std::min(static_cast<size_t>(data.size()), m_buffer->size());
        if (dropLen > 0) {
            m_buffer->skip(dropLen);
            emit errorOccurred(QStringLiteral("RX buffer overflow, dropped %1 bytes").arg(dropLen));
        }

        if (!m_buffer->write(data)) {
            emit fatalError(QStringLiteral("RX buffer saturated, incoming data lost"));
            return;
        }
    }
}

void SerialPortWorker::handleError(QSerialPort::SerialPortError error) {
    if (!m_serial) return;
    if (error == QSerialPort::NoError) return;

    const QString errorString = m_serial->errorString();
    m_serial->clearError();

    if (error == QSerialPort::ResourceError) {
        emit errorOccurred(QStringLiteral("Resource error: %1").arg(errorString));
        QTimer::singleShot(500, this, &SerialPortWorker::restartPort);
    } else {
        emit errorOccurred(QStringLiteral("Serial port error: %1").arg(errorString));
    }
}

void SerialPortWorker::watchdogTimeout() {
    if (!m_serial || !m_serial->isOpen()) return;

    if (!m_seenActivity) {
        // No traffic since open; stay idle instead of aggressive restart.
        return;
    }

    if (QDateTime::currentMSecsSinceEpoch() - m_lastActiveTime > kWatchdogSilentMs) {
        // Do not restart or log just for idleness; rely on error handling to recover.
        return;
    }
}

void SerialPortWorker::processPackets() {
    int drained = 0;
    QByteArray packet;

    while (true) {
        const size_t available = m_buffer->size();
        if (available == 0) {
            break;
        }

        const size_t readSize = std::min(static_cast<size_t>(kMaxChunkSize), available);
        if (!m_buffer->read(packet, readSize)) {
            break;
        }

        emit packetReady(packet);
        drained += static_cast<int>(readSize);

        if (drained >= kMaxDrainPerTick) {
            break;
        }
    }
}

bool SerialPortWorker::verifyChecksum(const QByteArray &packet) {
    if (packet.size() < 4) return false;

    quint8 sum = 0;
    for (int i = 2; i < packet.size() - 1; ++i) {
        sum += static_cast<quint8>(packet[i]);
    }
    return (sum == static_cast<quint8>(packet.back()));
}
