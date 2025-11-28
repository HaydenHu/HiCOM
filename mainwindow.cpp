#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDateTime>
#include <QCheckBox>
#include <QMessageBox>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSpinBox>
#include <QSerialPortInfo>
#include <QTimer>

#ifndef ENABLE_DEBUG_LOG
#define ENABLE_DEBUG_LOG 1
#endif

namespace {
QString formatAsHex(const QByteArray &data) {
    QString hex = data.toHex(' ').toUpper();
    return hex;
}

QByteArray parseHexString(const QString &text, bool *ok) {
    QByteArray result;
    QString cleaned = text;
    cleaned.remove(QRegularExpression(QStringLiteral("[^0-9A-Fa-f]")));
    if (cleaned.size() % 2 != 0) {
        cleaned.prepend('0');
    }
    result.reserve(cleaned.size() / 2);
    for (int i = 0; i < cleaned.size(); i += 2) {
        bool byteOk = false;
        const quint8 value = static_cast<quint8>(cleaned.mid(i, 2).toUInt(&byteOk, 16));
        if (!byteOk) {
            if (ok) *ok = false;
            return {};
        }
        result.append(static_cast<char>(value));
    }
    if (ok) *ok = true;
    return result;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_serialThread(new QThread(this))
    , m_serialWorker(new SerialPortWorker)
    , m_isWriting(false)
{
    ui->setupUi(this);
    m_sendTimer = new QTimer(this);
    m_sendTimer->setSingleShot(false);

    connect(ui->openBt, &QPushButton::clicked, this, &MainWindow::on_openButton_clicked);
    connect(ui->sendBt, &QPushButton::clicked, this, &MainWindow::on_sendButton_clicked);
    connect(ui->btnClearSend, &QPushButton::clicked, this, [this]() {
        ui->sendEdit->clear();
    });
    connect(ui->clearBt, &QPushButton::clicked, this, [this]() {
        ui->recvEdit->clear();
    });
    connect(ui->chkTimSend, &QCheckBox::toggled, this, [this](bool on) {
        m_autoSend = on;
        if (on) {
            const int interval = ui->txtSendMs->value();
            m_sendTimer->setInterval(interval);
            m_sendTimer->start();
        } else {
            m_sendTimer->stop();
        }
    });
    connect(ui->txtSendMs, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (m_autoSend) {
            m_sendTimer->setInterval(v);
        }
    });
    connect(m_sendTimer, &QTimer::timeout, this, [this]() {
        on_sendButton_clicked();
    });

    m_serialWorker->moveToThread(m_serialThread);
    connect(m_serialThread, &QThread::finished, m_serialWorker, &QObject::deleteLater);
    m_serialThread->start();

    connect(m_serialWorker, &SerialPortWorker::packetReady,
            this, &MainWindow::onPacketReceived, Qt::QueuedConnection);
    connect(m_serialWorker, &SerialPortWorker::errorOccurred,
            this, &MainWindow::onErrorOccurred, Qt::QueuedConnection);
    connect(m_serialWorker, &SerialPortWorker::fatalError,
            this, &MainWindow::onFatalError, Qt::QueuedConnection);
    connect(m_serialWorker, &SerialPortWorker::portOpened,
            this, &MainWindow::onPortOpened, Qt::QueuedConnection);
    connect(m_serialWorker, &SerialPortWorker::portClosed,
            this, &MainWindow::onPortClosed, Qt::QueuedConnection);
    connect(m_serialWorker, &SerialPortWorker::infoMessage,
            this, [this](const QString& msg) {
                appendDebug(msg);
            }, Qt::QueuedConnection);

    QTimer::singleShot(0, this, [this]() {
        QMetaObject::invokeMethod(m_serialWorker, "initializeSerialPort", Qt::QueuedConnection);
    });

    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        ui->serialCb->addItem(info.portName());
    }
}

MainWindow::~MainWindow()
{
    if (m_serialWorker) {
        QMetaObject::invokeMethod(m_serialWorker, "stopPort", Qt::QueuedConnection);
    }
    if (m_serialThread) {
        m_serialThread->quit();
        m_serialThread->wait();
    }
    delete ui;
}

void MainWindow::on_openButton_clicked()
{
    if (!m_isPortOpen) {
        SerialSettings settings = getCurrentSerialSettings();
        QMetaObject::invokeMethod(m_serialWorker, "startPort",
                                  Qt::QueuedConnection,
                                  Q_ARG(SerialSettings, settings));
    } else {
        QMetaObject::invokeMethod(m_serialWorker, "stopPort", Qt::QueuedConnection);
    }
}

void MainWindow::on_sendButton_clicked()
{
    const QString text = ui->sendEdit->toPlainText();
    QByteArray payload = buildSendPayload(text);
    if (payload.isEmpty() && !text.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("发送失败"), QStringLiteral("HEX格式无效，未发送。"));
        return;
    }
    writeData(payload);
}

SerialSettings MainWindow::getCurrentSerialSettings() const
{
    SerialSettings settings;
    settings.portName = ui->serialCb->currentText();
    settings.baudRate = static_cast<QSerialPort::BaudRate>(ui->baundrateCb->currentText().toInt());
    settings.dataBits = static_cast<QSerialPort::DataBits>(ui->databitCb->currentText().toInt());

    switch (ui->checkbitCb->currentIndex()) {
    case 1: settings.parity = QSerialPort::OddParity; break;
    case 2: settings.parity = QSerialPort::EvenParity; break;
    default: settings.parity = QSerialPort::NoParity; break;
    }

    switch (ui->stopbitCb->currentIndex()) {
    case 1: settings.stopBits = QSerialPort::OneAndHalfStop; break;
    case 2: settings.stopBits = QSerialPort::TwoStop; break;
    default: settings.stopBits = QSerialPort::OneStop; break;
    }
    return settings;
}

void MainWindow::writeData(const QByteArray &data)
{
    QMutexLocker locker(&m_queueMutex);
    m_writeQueue.append(data);
    if (m_isWriting) {
        return;
    }

    m_isWriting = true;
    locker.unlock();
    QMetaObject::invokeMethod(this, [this]() { processWriteQueue(); }, Qt::QueuedConnection);
}

void MainWindow::processWriteQueue()
{
    QByteArray data;
    {
        QMutexLocker locker(&m_queueMutex);
        if (m_writeQueue.isEmpty()) {
            m_isWriting = false;
            return;
        }
        data = m_writeQueue.takeFirst();
    }

    QMetaObject::invokeMethod(m_serialWorker, "writeToPort",
                              Qt::QueuedConnection,
                              Q_ARG(QByteArray, data));

    QMetaObject::invokeMethod(this, [this]() { processWriteQueue(); }, Qt::QueuedConnection);
}

void MainWindow::onPacketReceived(const QByteArray &packet)
{
    QString line;
    if (ui->chk_rev_time->isChecked()) {
        line += QDateTime::currentDateTime().toString(QStringLiteral("[HH:mm:ss.zzz] "));
    }
    if (ui->chk_rev_hex->isChecked()) {
        line += formatAsHex(packet).toHtmlEscaped();
    } else {
        line += QString::fromUtf8(packet).toHtmlEscaped();
    }
    if (ui->chk_rev_line->isChecked()) {
        line += QLatin1Char('\n');
    }
    ui->recvEdit->append(line);
}

void MainWindow::onErrorOccurred(const QString &error)
{
    QMessageBox::warning(this, "Serial Port Error", error);
    ui->recvEdit->append(QStringLiteral("<span style=\"color:red;\">[ERROR] %1</span>")
                             .arg(error.toHtmlEscaped()));
}

void MainWindow::onFatalError(const QString &error)
{
    QMessageBox::critical(this, "Serial Port Fatal Error", error);
    ui->recvEdit->append(QStringLiteral("<span style=\"color:red;\">[FATAL] %1</span>")
                             .arg(error.toHtmlEscaped()));
    m_isPortOpen = false;
    ui->openBt->setText(QStringLiteral("打开串口"));
}

void MainWindow::onPortOpened()
{
    m_isPortOpen = true;
    ui->openBt->setText(QStringLiteral("关闭串口"));
    ui->serialCb->setEnabled(false);
    ui->baundrateCb->setEnabled(false);
    ui->databitCb->setEnabled(false);
    ui->checkbitCb->setEnabled(false);
    ui->stopbitCb->setEnabled(false);
    appendDebug(QStringLiteral("Serial port opened successfully."));
}

void MainWindow::onPortClosed()
{
    m_isPortOpen = false;
    ui->openBt->setText(QStringLiteral("打开串口"));
    ui->serialCb->setEnabled(true);
    ui->baundrateCb->setEnabled(true);
    ui->databitCb->setEnabled(true);
    ui->checkbitCb->setEnabled(true);
    ui->stopbitCb->setEnabled(true);
    appendDebug(QStringLiteral("Serial port closed."));
}

QByteArray MainWindow::buildSendPayload(const QString &text) const
{
    QString content = text;
    if (ui->chk_send_line->isChecked()) {
        content.append(QLatin1Char('\n'));
    }

    if (ui->chk_send_hex->isChecked()) {
        bool ok = false;
        QByteArray data = parseHexString(content, &ok);
        if (!ok) return {};
        return data;
    }

    return content.toUtf8();
}

void MainWindow::appendDebug(const QString &text)
{
#if ENABLE_DEBUG_LOG
    ui->recvEdit->append(QStringLiteral("<span style=\"color:red;\">%1</span>")
                             .arg(text.toHtmlEscaped()));
#else
    Q_UNUSED(text);
#endif
}
