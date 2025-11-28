#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFontMetrics>
#include <QLabel>
#include <QMessageBox>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QStatusBar>
#include <QTextStream>
#include <QTimer>
#include <algorithm>

#ifndef ENABLE_DEBUG_LOG
#define ENABLE_DEBUG_LOG 1
#endif

namespace {
QString formatAsHex(const QByteArray &data) {
    return data.toHex(' ').toUpper();
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
    m_portPollTimer = new QTimer(this);
    m_portPollTimer->setSingleShot(false);
    m_portPollTimer->setInterval(1500);
    m_statusConn = new QLabel(this);
    m_statusRx = new QLabel(this);
    m_statusTx = new QLabel(this);
    m_enableDebug = (ENABLE_DEBUG_LOG != 0);
    updateStatusLabels();
    ui->statusbar->addWidget(m_statusConn);
    ui->statusbar->addPermanentWidget(m_statusRx);
    ui->statusbar->addPermanentWidget(m_statusTx);

    connect(ui->openBt, &QPushButton::clicked, this, &MainWindow::on_openButton_clicked);
    connect(ui->sendBt, &QPushButton::clicked, this, &MainWindow::on_sendButton_clicked);
    connect(ui->btnClearSend, &QPushButton::clicked, this, [this]() { ui->sendEdit->clear(); });
    connect(ui->clearBt, &QPushButton::clicked, this, [this]() { ui->recvEdit->clear(); });
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
    connect(m_sendTimer, &QTimer::timeout, this, [this]() { on_sendButton_clicked(); });
    connect(m_portPollTimer, &QTimer::timeout, this, [this]() { checkPortHotplug(); });

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
            this, [this](const QString& msg) { appendDebug(msg); }, Qt::QueuedConnection);

    QTimer::singleShot(0, this, [this]() {
        QMetaObject::invokeMethod(m_serialWorker, "initializeSerialPort", Qt::QueuedConnection);
    });
    refreshSerialPorts();
    m_portPollTimer->start();
    connect(ui->serialCb, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        updateSerialTooltip();
    });

    ui->btnSerialCheck->setText(QStringLiteral("保存记录"));
    connect(ui->btnSerialCheck, &QPushButton::clicked, this, [this]() { saveLogs(); });
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
        m_currentSettings = settings;
        m_hasCurrentSettings = true;
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
    const QVariant portData = ui->serialCb->currentData();
    settings.portName = portData.isValid() ? portData.toString() : ui->serialCb->currentText();
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
    m_txBytes += data.size();
    updateStatusLabels();
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
    m_rxBytes += packet.size();
    updateStatusLabels();

    QString line;
    if (ui->chk_rev_time->isChecked()) {
        const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("[HH:mm:ss.zzz] "));
        m_toggleTimestampColor = !m_toggleTimestampColor;
        const QString color = m_toggleTimestampColor ? QStringLiteral("#007aff") : QStringLiteral("#ff6a00");
        line += QStringLiteral("<span style=\"color:%1;\">%2</span> ").arg(color, ts.toHtmlEscaped());
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
    m_rxBytes = 0;
    m_txBytes = 0;
    updateStatusLabels();
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
    m_hasCurrentSettings = false;
    updateStatusLabels();
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
    if (!m_enableDebug) {
        return;
    }
    ui->recvEdit->append(QStringLiteral("<span style=\"color:red;\">%1</span>")
                             .arg(text.toHtmlEscaped()));
}

void MainWindow::refreshSerialPorts()
{
    const QVariant currentData = ui->serialCb->currentData();
    const QString currentText = ui->serialCb->currentText();

    ui->serialCb->clear();
    const int maxWidth = 160;
    const QFontMetrics fm(ui->serialCb->font());

    QStringList names;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        const QString full = QStringLiteral("%1-%2(VID:0x%3 PID:0x%4 MFR:%5 SN:%6)")
                                 .arg(info.portName(),
                                      info.description().isEmpty() ? QStringLiteral("Unknown") : info.description(),
                                      QString::number(info.vendorIdentifier(), 16).toUpper().rightJustified(4, '0'),
                                      QString::number(info.productIdentifier(), 16).toUpper().rightJustified(4, '0'),
                                      info.manufacturer().isEmpty() ? QStringLiteral("N/A") : info.manufacturer(),
                                      info.serialNumber().isEmpty() ? QStringLiteral("N/A") : info.serialNumber());
        const QString elided = fm.elidedText(full, Qt::ElideRight, maxWidth);
        ui->serialCb->addItem(elided, info.portName());
        const int idx = ui->serialCb->count() - 1;
        ui->serialCb->setItemData(idx, full, Qt::ToolTipRole);
        names << info.portName();
    }

    int restoreIndex = -1;
    if (currentData.isValid()) {
        restoreIndex = ui->serialCb->findData(currentData);
    } else if (!currentText.isEmpty()) {
        restoreIndex = ui->serialCb->findText(currentText);
    }
    if (restoreIndex >= 0) {
        ui->serialCb->setCurrentIndex(restoreIndex);
    } else if (ui->serialCb->count() > 0) {
        ui->serialCb->setCurrentIndex(0);
    }
    names.sort();
    m_knownPorts = names;
    updateSerialTooltip();
}

void MainWindow::updateSerialTooltip()
{
    const int idx = ui->serialCb->currentIndex();
    if (idx >= 0) {
        const QVariant tip = ui->serialCb->itemData(idx, Qt::ToolTipRole);
        if (tip.isValid()) {
            ui->serialCb->setToolTip(tip.toString());
            return;
        }
    }
    ui->serialCb->setToolTip(QString());
}

void MainWindow::updateStatusLabels()
{
    if (m_statusConn) {
        if (m_isPortOpen && m_hasCurrentSettings) {
            QString parityText;
            switch (m_currentSettings.parity) {
            case QSerialPort::EvenParity: parityText = QStringLiteral("E"); break;
            case QSerialPort::OddParity: parityText = QStringLiteral("O"); break;
            default: parityText = QStringLiteral("N"); break;
            }
            QString stopText;
            switch (m_currentSettings.stopBits) {
            case QSerialPort::OneAndHalfStop: stopText = QStringLiteral("1.5"); break;
            case QSerialPort::TwoStop: stopText = QStringLiteral("2"); break;
            default: stopText = QStringLiteral("1"); break;
            }
            const QString text = QStringLiteral("%1 | %2 %3%4%5")
                                     .arg(m_currentSettings.portName)
                                     .arg(m_currentSettings.baudRate)
                                     .arg(static_cast<int>(m_currentSettings.dataBits))
                                     .arg(parityText)
                                     .arg(stopText);
            m_statusConn->setText(text);
            m_statusConn->setStyleSheet(QStringLiteral("color: green;"));
        } else {
            m_statusConn->setText(QStringLiteral("未连接"));
            m_statusConn->setStyleSheet(QStringLiteral("color: red;"));
        }
    }
    if (m_statusRx) {
        m_statusRx->setText(QStringLiteral("RX: %1").arg(m_rxBytes));
    }
    if (m_statusTx) {
        m_statusTx->setText(QStringLiteral("TX: %1").arg(m_txBytes));
    }
}

void MainWindow::checkPortHotplug()
{
    QStringList names;
    for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
        names << info.portName();
    }
    names.sort();
    if (names != m_knownPorts) {
        refreshSerialPorts();
    }
}

void MainWindow::saveLogs()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存记录"),
        QDir::homePath() + QLatin1String("/hicom_log_") + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".txt",
        QStringLiteral("Text Files (*.txt);;All Files (*.*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("无法打开文件写入。"));
        return;
    }

    QTextStream out(&file);
    out << "===== Receive =====\n";
    out << ui->recvEdit->toPlainText() << "\n";
    out << "===== Send =====\n";
    out << ui->sendEdit->toPlainText() << "\n";
    file.close();

    QMessageBox::information(this, QStringLiteral("保存完成"), QStringLiteral("已保存到：\n") + path);
}
