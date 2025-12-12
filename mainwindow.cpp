#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMutexLocker>
#include <QPalette>
#include <QRegularExpression>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QTextEdit>
#include <QTextStream>
#include <QStringDecoder>
#include <QTimer>
#include <QVBoxLayout>
#include <QToolTip>
#include <limits>
#include <QStackedLayout>
#include <Qt3DCore/QEntity>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QCuboidMesh>
#include <Qt3DExtras/QConeMesh>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QCamera>
#include <Qt3DExtras/QForwardRenderer>
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
    m_statusMatch = new QLabel(this);
    m_statusMatch->setTextFormat(Qt::PlainText);
    m_statusMatch->setMinimumWidth(200);
    m_statusMatch->setAlignment(Qt::AlignCenter);
    m_recvSearchPanel = new QWidget(this);
    {
        auto h = new QHBoxLayout(m_recvSearchPanel);
        h->setContentsMargins(4, 0, 4, 0);
        h->setSpacing(4);
        QLabel* lbl = new QLabel(QString::fromUtf8(u8"搜索"), m_recvSearchPanel);
        m_recvSearchEdit = new QLineEdit(m_recvSearchPanel);
        m_recvSearchEdit->setPlaceholderText(QString::fromUtf8(u8"在接收区查找，按 Enter 查找下一个"));
        m_recvSearchPrev = new QToolButton(m_recvSearchPanel);
        m_recvSearchPrev->setText(QStringLiteral("▲"));
        m_recvSearchNext = new QToolButton(m_recvSearchPanel);
        m_recvSearchNext->setText(QStringLiteral("▼"));
        m_recvSearchClose = new QToolButton(m_recvSearchPanel);
        m_recvSearchClose->setText(QStringLiteral("✕"));
        h->addWidget(lbl);
        h->addWidget(m_recvSearchEdit, 1);
        h->addWidget(m_recvSearchPrev);
        h->addWidget(m_recvSearchNext);
        h->addWidget(m_recvSearchClose);
        m_recvSearchPanel->setVisible(false);
    }
    m_enableDebug = (ENABLE_DEBUG_LOG != 0);
    updateStatusLabels();
    ui->statusbar->addWidget(m_statusConn);
    ui->statusbar->addWidget(m_statusMatch, 1);
    ui->statusbar->addPermanentWidget(m_statusRx);
    ui->statusbar->addPermanentWidget(m_statusTx);
    m_recvFontPt = ui->recvEdit->font().pointSize();
    m_sendFontPt = ui->sendEdit->font().pointSize();
    if (m_recvFontPt <= 0) m_recvFontPt = 10;
    if (m_sendFontPt <= 0) m_sendFontPt = 10;
    ui->recvEdit->installEventFilter(this);
    ui->sendEdit->installEventFilter(this);
    ui->recvEdit->viewport()->installEventFilter(this);
    ui->sendEdit->viewport()->installEventFilter(this);
    if (ui->recvEdit->verticalScrollBar()) ui->recvEdit->verticalScrollBar()->installEventFilter(this);
    if (ui->recvEdit->horizontalScrollBar()) ui->recvEdit->horizontalScrollBar()->installEventFilter(this);
    if (ui->sendEdit->verticalScrollBar()) ui->sendEdit->verticalScrollBar()->installEventFilter(this);
    if (ui->sendEdit->horizontalScrollBar()) ui->sendEdit->horizontalScrollBar()->installEventFilter(this);

    connect(ui->openBt, &QPushButton::clicked, this, &MainWindow::on_openButton_clicked);
    connect(ui->sendBt, &QPushButton::clicked, this, &MainWindow::on_sendButton_clicked);
    connect(ui->btnClearSend, &QPushButton::clicked, this, [this]() {
        ui->sendEdit->clear();
        m_txBytes = 0;
        updateStatusLabels();
    });
    connect(ui->clearBt, &QPushButton::clicked, this, [this]() {
        ui->recvEdit->clear();
        m_recvAutoFollow = true;
        m_rxBytes = 0;
        updateStatusLabels();
        m_utf8Decoder = QStringDecoder(QStringDecoder::Utf8);
    });
    // 搜索栏与快捷键
    QShortcut* findShortcut = new QShortcut(QKeySequence::Find, ui->recvEdit);
    connect(findShortcut, &QShortcut::activated, this, [this]() { showRecvSearch(); });
    connect(m_recvSearchClose, &QToolButton::clicked, this, [this]() { hideRecvSearch(); });
    connect(m_recvSearchNext, &QToolButton::clicked, this, [this]() { findInRecv(false); });
    connect(m_recvSearchPrev, &QToolButton::clicked, this, [this]() { findInRecv(true); });
    connect(m_recvSearchEdit, &QLineEdit::returnPressed, this, [this]() { findInRecv(false); });
    connect(m_recvSearchEdit, &QLineEdit::textChanged, this, [this]() { updateRecvSearchHighlights(); });
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

    // Attitude worker thread for 3D display
    m_attThread = new QThread(this);
    m_attWorker = new AttitudeWorker;
    m_attWorker->moveToThread(m_attThread);
    connect(m_attWorker, &AttitudeWorker::attitudeReady,
            this, &MainWindow::updateAttitude, Qt::QueuedConnection);
    m_attThread->start();

    // waveform worker/thread
    m_waveThread = new QThread(this);
    m_waveWorker = new WaveformWorker;
    m_waveWorker->moveToThread(m_waveThread);
    connect(m_waveWorker, &WaveformWorker::dataReady, this, &MainWindow::updateWaveform, Qt::QueuedConnection);
    m_waveThread->start();

    // setup UI extras
    setupWaveformTab();

    QTimer::singleShot(0, this, [this]() {
        QMetaObject::invokeMethod(m_serialWorker, "initializeSerialPort", Qt::QueuedConnection);
    });
    refreshSerialPorts();
    m_portPollTimer->start();
    connect(ui->serialCb, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        updateSerialTooltip();
    });
    if (QScrollBar* vs = ui->recvEdit->verticalScrollBar()) {
        auto syncFollow = [this, vs](int value) {
            if (m_inRecvAppend) return;
            const bool atBottom = (value >= vs->maximum());
            m_recvAutoFollow = atBottom;
            if (atBottom) {
                vs->setStyleSheet(QString());
            }
        };
        connect(vs, &QScrollBar::valueChanged, this, syncFollow);
        connect(vs, &QScrollBar::sliderReleased, this, [this, vs, syncFollow]() { syncFollow(vs->value()); });
        connect(vs, &QScrollBar::sliderPressed, this, [this]() { m_recvAutoFollow = false; });
        connect(vs, &QAbstractSlider::actionTriggered, this, [this, vs](int action) {
            if (m_inRecvAppend) return;
            if (action != QAbstractSlider::SliderNoAction && action != QAbstractSlider::SliderToMaximum) {
                m_recvAutoFollow = false;
            }
            if (vs->value() >= vs->maximum()) {
                m_recvAutoFollow = true;
            }
        });
    }

    ui->btnSerialCheck->setText(QString::fromUtf8(u8"保存记录"));
    connect(ui->btnSerialCheck, &QPushButton::clicked, this, [this]() { saveLogs(); });

    // 默认正则
    m_waveRegexList = {QStringLiteral("(-?\\d+(?:\\.\\d+)?)")};
    m_attRegex = QStringLiteral("([-+]?\\d+(?:\\.\\d+)?)[,\\s]+([-+]?\\d+(?:\\.\\d+)?)[,\\s]+([-+]?\\d+(?:\\.\\d+)?)");
    m_customRegexEnableSpec = QStringLiteral("0");

    // 右上角格式按钮，打开设置弹窗
    m_formatBtn = new QToolButton(this);
    m_formatBtn->setText(QString::fromUtf8(u8"⚙"));
    m_formatBtn->setToolTip(QString::fromUtf8(u8"格式设置"));
    m_formatBtn->setAutoRaise(true);
    m_formatBtn->setFixedSize(24, 24);
    connect(m_formatBtn, &QToolButton::clicked, this, &MainWindow::openFormatDialog);

    // 右上角工具栏：搜索 + 格式按钮
    if (ui->tabWidget) {
        QWidget* corner = new QWidget(this);
        auto cornerLayout = new QHBoxLayout(corner);
        cornerLayout->setContentsMargins(0, 0, 0, 0);
        cornerLayout->setSpacing(4);
        cornerLayout->addWidget(m_recvSearchPanel);
        cornerLayout->addWidget(m_formatBtn);
        ui->tabWidget->setCornerWidget(corner, Qt::TopRightCorner);
    } else {
        ui->statusbar->addPermanentWidget(m_recvSearchPanel);
        ui->statusbar->addPermanentWidget(m_formatBtn);
    }

    setup3DTab();
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
    if (m_attThread) {
        m_attThread->quit();
        m_attThread->wait();
        delete m_attWorker;
    }
    if (m_waveThread) {
        m_waveThread->quit();
        m_waveThread->wait();
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
        QMessageBox::warning(this,
                             QString::fromUtf8(u8"发送失败"),
                             QString::fromUtf8(u8"HEX格式无效，未发送。"));
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
    if (!m_isPortOpen) {
        appendDebug(QStringLiteral("Send skipped: port not open"));
        return;
    }

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
    QVector<double> waveValues;
    bool waveParsed = false;
    const QString decoded = decodeTextSmart(packet);
    const QString raw = decoded.trimmed();
    if (m_useWaveRegex) {
        if (tryParseWaveValues(raw, waveValues) && !waveValues.isEmpty()) {
            updateWaveformValues(waveValues);
            waveParsed = true;
        }
    }
    // 当未启用波形正则或未能成功解析时，不再按字节值灌入波形，避免显示三角波

    if (m_attLabel) {
        if (!raw.isEmpty()) {
            QString display = raw;
            if (display.size() > 80) display = display.left(77) + "...";
            const QStringList parts = display.split(QRegularExpression(QStringLiteral("[,\\s]+")),
                                                    Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                display = QStringLiteral("Roll: %1   Pitch: %2   Yaw: %3")
                              .arg(parts.at(0))
                              .arg(parts.at(1))
                              .arg(parts.at(2));
            }
            m_attLabel->setText(display);
        }
    }
    if (m_attWorker) {
        double r, p, y;
        if (tryParseAttitude(decoded, r, p, y)) {
            QMetaObject::invokeMethod(m_attWorker, "appendAttitude", Qt::QueuedConnection,
                                      Q_ARG(double, r), Q_ARG(double, p), Q_ARG(double, y));
        }
    }

    m_rxBytes += packet.size();
    updateStatusLabels();
    updateCustomMatchDisplay(raw);

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
        QString htmlBody = decoded.toHtmlEscaped();
        htmlBody.replace(QStringLiteral("\r\n"), QStringLiteral("<br/>"));
        htmlBody.replace(QStringLiteral("\n"), QStringLiteral("<br/>"));
        line += htmlBody;
    }
    if (ui->chk_rev_line->isChecked()) {
        line += QStringLiteral("<br/>");
    }

    QScrollBar* vs = ui->recvEdit->verticalScrollBar();
    int restorePos = -1;
    if (vs && !m_recvAutoFollow) restorePos = vs->value();

    m_inRecvAppend = true;
    ui->recvEdit->append(line);
    m_inRecvAppend = false;

    if (m_recvAutoFollow) {
        QTextCursor c = ui->recvEdit->textCursor();
        c.movePosition(QTextCursor::End);
        ui->recvEdit->setTextCursor(c);
        ui->recvEdit->ensureCursorVisible();
        if (vs) vs->setStyleSheet(QStringLiteral(
            "QScrollBar:vertical {background: #e6f5e6;}"
            "QScrollBar::handle:vertical {background: #1f5c1f; min-height: 24px; border-radius: 4px;}"));
    } else if (vs && restorePos >= 0) {
        QSignalBlocker block(vs);
        vs->setValue(restorePos);
        vs->setStyleSheet(QStringLiteral(
            "QScrollBar:vertical {background: #e8ffe8;}"
            "QScrollBar::handle:vertical {background: #3fa34a; min-height: 24px; border-radius: 4px;}"));
    }
    if (vs) {
        const int token = ++m_recvColorToken;
        QScrollBar* vsPtr = vs;
        QTimer::singleShot(800, this, [this, vsPtr, token]() {
            if (token == m_recvColorToken && ui && ui->recvEdit && ui->recvEdit->verticalScrollBar() == vsPtr) {
                vsPtr->setStyleSheet(QString());
            }
        });
    }
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
    ui->openBt->setText(QStringLiteral("Open Port"));
}

void MainWindow::onPortOpened()
{
    m_isPortOpen = true;
    m_recvAutoFollow = true;
    m_inRecvAppend = false;
    m_utf8Decoder = QStringDecoder(QStringDecoder::Utf8);
    updateRecvSearchHighlights();
    if (QScrollBar* vs = ui->recvEdit->verticalScrollBar()) {
        vs->setValue(vs->maximum());
    }
    QTextCursor c = ui->recvEdit->textCursor();
    c.movePosition(QTextCursor::End);
    ui->recvEdit->setTextCursor(c);
    ui->recvEdit->ensureCursorVisible();
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
    m_recvAutoFollow = true;
    m_inRecvAppend = false;
    m_utf8Decoder = QStringDecoder(QStringDecoder::Utf8);
    updateRecvSearchHighlights();
    ui->openBt->setText(QStringLiteral("打开串口"));
    ui->serialCb->setEnabled(true);
    ui->baundrateCb->setEnabled(true);
    ui->databitCb->setEnabled(true);
    ui->checkbitCb->setEnabled(true);
    ui->stopbitCb->setEnabled(true);
    m_hasCurrentSettings = false;
    {
        QMutexLocker locker(&m_queueMutex);
        m_writeQueue.clear();
        m_isWriting = false;
    }
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

    // 文本模式?UTF-8 编码发送，emoji/中文等多字节字符才能完整传输
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
    const int maxWidth = 165;
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
            m_statusConn->setText(QString::fromUtf8(u8"未连接"));
            m_statusConn->setStyleSheet(QStringLiteral("color: red;"));
        }
    }
    if (m_statusRx) {
        m_statusRx->setText(QStringLiteral("RX: %1").arg(m_rxBytes));
    }
    if (m_statusMatch && !m_isPortOpen) {
        m_statusMatch->clear();
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
        this, QString::fromUtf8(u8"保存记录"),
        QDir::homePath() + QLatin1String("/hicom_log_") + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".txt",
        QString::fromUtf8(u8"文本文件 (*.txt);;所有文件 (*.*)"));
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QString::fromUtf8(u8"保存失败"), QString::fromUtf8(u8"无法打开文件写入。"));
        return;
    }

    QTextStream out(&file);
    out << "===== Receive =====\n";
    out << ui->recvEdit->toPlainText() << "\n";
    out << "===== Send =====\n";
    out << ui->sendEdit->toPlainText() << "\n";
    file.close();

    QMessageBox::information(this, QString::fromUtf8(u8"保存完成"), QString::fromUtf8(u8"已保存到：\n") + path);
}
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    const bool isRecv = (watched == ui->recvEdit || watched == ui->recvEdit->viewport() ||
                         watched == ui->recvEdit->verticalScrollBar() || watched == ui->recvEdit->horizontalScrollBar());
    const bool isSend = (watched == ui->sendEdit || watched == ui->sendEdit->viewport() ||
                         watched == ui->sendEdit->verticalScrollBar() || watched == ui->sendEdit->horizontalScrollBar());
    const bool isWave = (watched == m_wavePlot);

    if ((isRecv || isSend) && event->type() == QEvent::Wheel) {
        QWheelEvent *wheel = static_cast<QWheelEvent*>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            const int delta = wheel->angleDelta().y();
            const int step = (delta > 0) ? 1 : -1;
            const int minSize = 8;
            const int maxSize = 40;
            if (isRecv) {
                m_recvFontPt = std::clamp(m_recvFontPt + step, minSize, maxSize);
                QFont f = ui->recvEdit->font();
                f.setPointSize(m_recvFontPt);
                ui->recvEdit->setFont(f);
            } else {
                m_sendFontPt = std::clamp(m_sendFontPt + step, minSize, maxSize);
                QFont f = ui->sendEdit->font();
                f.setPointSize(m_sendFontPt);
                ui->sendEdit->setFont(f);
            }
            return true; // consume to avoid scroll
        } else if (isRecv) {
            // 用户滚动接收区：立即暂停自动跟随
            m_recvAutoFollow = false;
            return false;
        }
    }
    if (isRecv && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove)) {
        if (QMouseEvent* me = static_cast<QMouseEvent*>(event)) {
            if (me->buttons() & Qt::LeftButton) {
                m_recvAutoFollow = false;
            }
        }
    }
    if (isWave) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton && !(me->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier))) {
                m_waveAutoFollow = false;
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (m_waveGraph && !m_waveData.isEmpty()) {
                const double xCoord = m_wavePlot->xAxis->pixelToCoord(me->pos().x());
                double bestDist = std::numeric_limits<double>::max();
                QCPGraphData best;
                for (const auto &d : m_waveData) {
                    double dist = std::abs(d.key - xCoord);
                    if (dist < bestDist) {
                        bestDist = dist;
                        best = d;
                    }
                }
                if (bestDist != std::numeric_limits<double>::max()) {
                    QString tip = QStringLiteral("x: %1\ny: %2")
                                      .arg(QString::number(best.key, 'f', 0))
                                      .arg(QString::number(best.value, 'f', 3));
                    QToolTip::showText(me->globalPos(), tip, m_wavePlot);
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_waveAutoFollow) return false;
            if (!m_waveData.isEmpty() && m_wavePlot && m_wavePlot->xAxis) {
                const double lastX = m_waveData.last().key;
                const double viewRight = m_wavePlot->xAxis->range().upper;
                if (viewRight >= lastX - 1e-6) {
                    m_waveAutoFollow = true;
                }
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

QString MainWindow::decodeTextSmart(const QByteArray &data) const
{
    // Streamed UTF-8 decoder to handle split packets without回退到本地编码
    QString out = m_utf8Decoder.decode(data);
    return out;
}

bool MainWindow::tryParseWaveValues(const QString &text, QVector<double> &values) const
{
    values.clear();
    if (!m_useWaveRegex || m_waveRegexList.isEmpty()) return false;

    for (const QString &pattern : m_waveRegexList) {
        if (pattern.trimmed().isEmpty()) continue;
        QRegularExpression re(pattern);
        QRegularExpressionMatchIterator it = re.globalMatch(text);
        QVector<double> tmp;
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            QString numStr;
            if (m.lastCapturedIndex() >= 1) {
                numStr = m.captured(1);
            } else {
                numStr = m.captured(0);
            }
            bool ok = false;
            double v = numStr.toDouble(&ok);
            if (ok) tmp.append(v);
        }
        if (!tmp.isEmpty()) {
            values = tmp;
            return true;
        }
    }
    return false;
}

QVector<int> MainWindow::parseIndexSpec(const QString &spec, int maxCount) const
{
    QVector<int> result;
    const QStringList tokens = spec.split(',', Qt::SkipEmptyParts);
    for (QString token : tokens) {
        token = token.trimmed();
        if (token.contains('-')) {
            const QStringList parts = token.split('-', Qt::SkipEmptyParts);
            if (parts.size() == 2) {
                bool ok1 = false, ok2 = false;
                int a = parts.at(0).toInt(&ok1);
                int b = parts.at(1).toInt(&ok2);
                if (ok1 && ok2) {
                    if (a > b) std::swap(a, b);
                    for (int i = a; i <= b; ++i) {
                        if (i >= 1 && i <= maxCount) result.append(i);
                    }
                }
            }
        } else {
            bool ok = false;
            int v = token.toInt(&ok);
            if (ok && v >= 1 && v <= maxCount) result.append(v);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

void MainWindow::updateCustomMatchDisplay(const QString &text)
{
    if (!m_statusMatch) return;
    if (!m_isPortOpen) {
        m_statusMatch->clear();
        return;
    }
    if (m_customRegexList.isEmpty()) {
        m_statusMatch->clear();
        return;
    }
    const QString enableSpec = m_customRegexEnableSpec.trimmed();
    if (enableSpec == QStringLiteral("0")) {
        m_statusMatch->clear();
        return;
    }
    const QVector<int> enabled = parseIndexSpec(enableSpec, m_customRegexList.size());
    QVector<int> finalIdx = enabled;
    if (finalIdx.isEmpty()) {
        finalIdx.reserve(m_customRegexList.size());
        for (int i = 0; i < m_customRegexList.size(); ++i) finalIdx.append(i + 1);
    }

    QStringList hits;
    for (int idx : finalIdx) {
        const QString pattern = m_customRegexList.value(idx - 1).trimmed();
        if (pattern.isEmpty()) continue;
        QRegularExpression re(pattern, QRegularExpression::MultilineOption);
        if (!re.isValid()) continue;
        QRegularExpressionMatchIterator it = re.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            QString captured;
            if (m.lastCapturedIndex() >= 1) {
                captured = m.captured(1);
            } else {
                captured = m.captured(0);
            }
            if (!captured.isEmpty()) hits << captured;
        }
    }

    if (hits.isEmpty()) {
        m_statusMatch->clear();
    } else {
        QString joined = hits.join(QString::fromUtf8(u8" | "));
        if (joined.size() > 200) {
            joined = joined.left(197) + QStringLiteral("...");
        }
        m_statusMatch->setText(joined);
    }
}

void MainWindow::showRecvSearch()
{
    if (!m_recvSearchPanel || !m_recvSearchEdit) return;
    m_recvSearchPanel->setVisible(true);
    m_recvSearchEdit->setFocus(Qt::ShortcutFocusReason);
    m_recvSearchEdit->selectAll();
    updateRecvSearchHighlights();
}

void MainWindow::hideRecvSearch()
{
    if (!m_recvSearchPanel || !m_recvSearchEdit) return;
    m_recvSearchPanel->setVisible(false);
    m_recvSearchEdit->clear();
    ui->recvEdit->setExtraSelections({});
}

void MainWindow::updateRecvSearchHighlights()
{
    if (!ui || !ui->recvEdit) return;
    const QString pattern = m_recvSearchEdit ? m_recvSearchEdit->text() : QString();
    QList<QTextEdit::ExtraSelection> sels;
    if (!pattern.isEmpty()) {
        QRegularExpression re(QRegularExpression::escape(pattern), QRegularExpression::CaseInsensitiveOption);
        if (re.isValid()) {
            QTextDocument* doc = ui->recvEdit->document();
            QTextCursor c(doc);
            while (true) {
                c = doc->find(re, c);
                if (c.isNull()) break;
                QTextEdit::ExtraSelection s;
                s.cursor = c;
                QTextCharFormat fmt;
                fmt.setBackground(QColor(255, 230, 128));
                s.format = fmt;
                sels.append(s);
            }
        }
    }
    ui->recvEdit->setExtraSelections(sels);
}

void MainWindow::findInRecv(bool backward)
{
    if (!ui || !ui->recvEdit) return;
    const QString pattern = m_recvSearchEdit ? m_recvSearchEdit->text() : QString();
    if (pattern.isEmpty()) {
        updateRecvSearchHighlights();
        return;
    }

    QTextDocument::FindFlags flags;
    if (backward) flags |= QTextDocument::FindBackward;

    QTextCursor cursor = ui->recvEdit->textCursor();
    if (!backward) {
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor);
    } else {
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor);
    }
    QTextCursor found = ui->recvEdit->document()->find(QRegularExpression(QRegularExpression::escape(pattern)), cursor, flags);
    if (!found.isNull()) {
        ui->recvEdit->setTextCursor(found);
        ui->recvEdit->ensureCursorVisible();
        m_recvAutoFollow = false;
    }
    updateRecvSearchHighlights();
}

void MainWindow::openFormatDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8(u8"格式设置"));
    QVBoxLayout* v = new QVBoxLayout(&dlg);

    QCheckBox* waveEnable = new QCheckBox(QString::fromUtf8(u8"启用自定义波形正则（一行一个，使用第一个捕获组或整段匹配）"), &dlg);
    waveEnable->setChecked(m_useWaveRegex);
    v->addWidget(waveEnable);

    QPlainTextEdit* waveEdit = new QPlainTextEdit(&dlg);
    waveEdit->setPlaceholderText(QString::fromUtf8(u8"例：(-?\\\\d+(?:\\\\.\\\\d+)?)"));
    waveEdit->setPlainText(m_waveRegexList.join(QStringLiteral("\n")));
    waveEdit->setFixedHeight(100);
    v->addWidget(waveEdit);

    QCheckBox* attEnable = new QCheckBox(QString::fromUtf8(u8"启用自定义姿态正则（需至少3个捕获组，依次为Roll/Pitch/Yaw）"), &dlg);
    attEnable->setChecked(m_useAttRegex);
    v->addWidget(attEnable);

    QLineEdit* attEdit = new QLineEdit(&dlg);
    attEdit->setPlaceholderText(QString::fromUtf8(u8"例：([-+]?\\\\d+(?:\\\\.\\\\d+)?)[,\\\\s]+([-+]?\\\\d+(?:\\\\.\\\\d+)?)[,\\\\s]+([-+]?\\\\d+(?:\\\\.\\\\d+)?)"));
    attEdit->setText(m_attRegex);
    v->addWidget(attEdit);

    QLabel* customLabel = new QLabel(QString::fromUtf8(u8"自定义匹配正则（每行一条，优先使用第一个捕获组，否则使用整段匹配；结果在状态栏以“|”分隔显示）"), &dlg);
    customLabel->setWordWrap(true);
    v->addWidget(customLabel);

    QPlainTextEdit* customEdit = new QPlainTextEdit(&dlg);
    customEdit->setPlaceholderText(QString::fromUtf8(u8"示例：\\nvolt: ([\\\\d.]+)\\namp: ([\\\\d.]+)"));
    customEdit->setPlainText(m_customRegexList.join(QStringLiteral("\n")));
    customEdit->setFixedHeight(120);
    v->addWidget(customEdit);

    QLineEdit* customEnableEdit = new QLineEdit(&dlg);
    customEnableEdit->setPlaceholderText(QString::fromUtf8(u8"启用哪些规则，例如 1-3 或 1,2,5"));
    customEnableEdit->setText(m_customRegexEnableSpec);
    v->addWidget(customEnableEdit);

    QHBoxLayout* btns = new QHBoxLayout;
    QPushButton* resetBtn = new QPushButton(QString::fromUtf8(u8"恢复默认"), &dlg);
    QPushButton* okBtn = new QPushButton(QString::fromUtf8(u8"确定"), &dlg);
    QPushButton* cancelBtn = new QPushButton(QString::fromUtf8(u8"取消"), &dlg);
    btns->addStretch();
    btns->addWidget(resetBtn);
    btns->addWidget(okBtn);
    btns->addWidget(cancelBtn);
    v->addLayout(btns);

    connect(resetBtn, &QPushButton::clicked, [&]() {
        waveEnable->setChecked(false);
        attEnable->setChecked(false);
        waveEdit->setPlainText(QStringLiteral("(-?\d+(?:\.\d+)?)"));
        attEdit->setText(QStringLiteral("([-+]?\d+(?:\.\d+)?)[,\s]+([-+]?\d+(?:\.\d+)?)[,\s]+([-+]?\d+(?:\.\d+)?)"));
        customEdit->clear();
        customEnableEdit->setText(QStringLiteral("0"));
    });
    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        m_useWaveRegex = waveEnable->isChecked();
        m_useAttRegex = attEnable->isChecked();
        m_waveRegexList = waveEdit->toPlainText().split("\n", Qt::SkipEmptyParts);
        for (QString &s : m_waveRegexList) s = s.trimmed();
        m_attRegex = attEdit->text().trimmed();
        m_customRegexList = customEdit->toPlainText().split("\n", Qt::SkipEmptyParts);
        for (QString &s : m_customRegexList) s = s.trimmed();
        m_customRegexEnableSpec = customEnableEdit->text().trimmed();
        updateCustomMatchDisplay(QString());
    }
}
void MainWindow::setupWaveformTab()
{
    QWidget *waveTab = ui->tabWidget->findChild<QWidget*>("tab_waveform");
    if (!waveTab) {
        waveTab = new QWidget;
        ui->tabWidget->insertTab(1, waveTab, QString::fromUtf8(u8"波形"));
    }

    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(waveTab->layout());
    if (!layout) {
        layout = new QVBoxLayout(waveTab);
        layout->setContentsMargins(0, 0, 0, 0);
        waveTab->setLayout(layout);
    } else {
        while (QLayoutItem *item = layout->takeAt(0)) {
            if (QWidget *w = item->widget()) {
                w->deleteLater();
            }
            delete item;
        }
    }

    m_wavePlot = new QCustomPlot(waveTab);
    layout->addWidget(m_wavePlot);

    m_waveGraph = m_wavePlot->addGraph();
    m_waveGraph->setPen(QPen(Qt::green));
    m_wavePlot->xAxis->setLabel("Sample");
    m_wavePlot->yAxis->setLabel("Value");
    m_wavePlot->yAxis->setRange(0, 260);
    m_wavePlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // 减少X轴刻度密度，避免拥挤
    {
        QSharedPointer<QCPAxisTicker> ticker(new QCPAxisTicker);
        ticker->setTickStepStrategy(QCPAxisTicker::tssMeetTickCount);
        ticker->setTickCount(6);
        m_wavePlot->xAxis->setTicker(ticker);
        m_wavePlot->xAxis->setNumberFormat("f");
        m_wavePlot->xAxis->setNumberPrecision(0);
    }

    // Match background to current theme
    const QColor bg = waveTab->palette().color(QPalette::Base);
    m_wavePlot->setBackground(bg);
    if (m_wavePlot->axisRect()) {
        m_wavePlot->axisRect()->setBackground(bg);
    }

    connect(m_wavePlot, &QCustomPlot::mouseDoubleClick, this, [this]() {
        m_waveAutoFollow = true;
    });

    m_wavePlot->installEventFilter(this);
}

void MainWindow::updateWaveform(const QVector<QPointF> &points)
{
    if (!m_waveGraph) return;
    for (const QPointF &p : points) {
        m_waveData.append(QCPGraphData{p.x(), p.y()});
    }
    if (m_waveData.size() > m_waveMaxPoints) {
        m_waveData.erase(m_waveData.begin(), m_waveData.begin() + (m_waveData.size() - m_waveMaxPoints));
    }
    m_waveGraph->data()->set(m_waveData, true);
    if (!m_waveData.isEmpty()) {
        if (m_waveAutoFollow) {
            const double xmax = m_waveData.last().key;
            const double xmin = std::max(0.0, xmax - m_waveViewWidth);
            m_waveRangeUpdating = true;
            m_wavePlot->xAxis->setRange(xmin, xmax);
            m_waveRangeUpdating = false;
        }
    }
    m_wavePlot->replot(QCustomPlot::rpQueuedReplot);
}

void MainWindow::updateWaveformValues(const QVector<double> &values)
{
    if (!m_waveGraph) return;
    for (double v : values) {
        m_waveData.append(QCPGraphData{m_waveX, v});
        m_waveX += 1.0;
    }
    if (m_waveData.size() > m_waveMaxPoints) {
        m_waveData.erase(m_waveData.begin(), m_waveData.begin() + (m_waveData.size() - m_waveMaxPoints));
    }
    m_waveGraph->data()->set(m_waveData, true);
    if (!m_waveData.isEmpty() && m_waveAutoFollow) {
        const double xmax = m_waveData.last().key;
        const double xmin = std::max(0.0, xmax - m_waveViewWidth);
        m_waveRangeUpdating = true;
        m_wavePlot->xAxis->setRange(xmin, xmax);
        m_waveRangeUpdating = false;
    }
    m_wavePlot->replot(QCustomPlot::rpQueuedReplot);
}

void MainWindow::setup3DTab()
{
    m_tab3d = ui->tabWidget->findChild<QWidget*>("tab_3D");
    if (!m_tab3d) {
        return;
    }

    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(m_tab3d->layout());
    if (!layout) {
        layout = new QVBoxLayout(m_tab3d);
        layout->setContentsMargins(0, 0, 0, 0);
        m_tab3d->setLayout(layout);
    } else {
        while (QLayoutItem *item = layout->takeAt(0)) {
            if (QWidget *w = item->widget()) w->deleteLater();
            delete item;
        }
    }

    m_3dWindow = new Qt3DExtras::Qt3DWindow;
    m_3dWindow->defaultFrameGraph()->setClearColor(QColor(30, 30, 35));
    m_3dRoot = new Qt3DCore::QEntity;

    auto cam = m_3dWindow->camera();
    cam->lens()->setPerspectiveProjection(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    cam->setPosition(QVector3D(0.0f, 0.0f, 12.0f));
    cam->setViewCenter(QVector3D(0, 0, 0));

    auto camController = new Qt3DExtras::QOrbitCameraController(m_3dRoot);
    camController->setCamera(cam);
    camController->setLinearSpeed(50.0f);
    camController->setLookSpeed(180.0f);

    // Simple airplane-like model
    m_modelEntity = new Qt3DCore::QEntity(m_3dRoot);
    auto bodyMesh = new Qt3DExtras::QCuboidMesh;
    bodyMesh->setXExtent(1.0f);
    bodyMesh->setYExtent(0.3f);
    bodyMesh->setZExtent(3.0f);
    auto bodyMaterial = new Qt3DExtras::QPhongMaterial;
    bodyMaterial->setDiffuse(QColor(80, 180, 255));
    m_modelTransform = new Qt3DCore::QTransform;
    m_modelTransform->setRotation(QQuaternion::fromEulerAngles(0, 0, 0));
    m_modelEntity->addComponent(bodyMesh);
    m_modelEntity->addComponent(bodyMaterial);
    m_modelEntity->addComponent(m_modelTransform);

    auto wingEntity = new Qt3DCore::QEntity(m_modelEntity);
    auto wingMesh = new Qt3DExtras::QCuboidMesh;
    wingMesh->setXExtent(4.0f);
    wingMesh->setYExtent(0.1f);
    wingMesh->setZExtent(0.5f);
    auto wingTransform = new Qt3DCore::QTransform;
    wingTransform->setTranslation(QVector3D(0, 0, 0));
    wingEntity->addComponent(wingMesh);
    wingEntity->addComponent(wingTransform);
    wingEntity->addComponent(bodyMaterial);

    auto tailEntity = new Qt3DCore::QEntity(m_modelEntity);
    auto tailMesh = new Qt3DExtras::QCuboidMesh;
    tailMesh->setXExtent(1.0f);
    tailMesh->setYExtent(0.1f);
    tailMesh->setZExtent(0.8f);
    auto tailTransform = new Qt3DCore::QTransform;
    tailTransform->setTranslation(QVector3D(0, 0.2f, -1.5f));
    tailEntity->addComponent(tailMesh);
    tailEntity->addComponent(tailTransform);
    tailEntity->addComponent(bodyMaterial);

    auto vtailEntity = new Qt3DCore::QEntity(m_modelEntity);
    auto vtailMesh = new Qt3DExtras::QCuboidMesh;
    vtailMesh->setXExtent(0.2f);
    vtailMesh->setYExtent(0.8f);
    vtailMesh->setZExtent(0.2f);
    auto vtailTransform = new Qt3DCore::QTransform;
    vtailTransform->setTranslation(QVector3D(0, 0.5f, -1.5f));
    vtailEntity->addComponent(vtailMesh);
    vtailEntity->addComponent(vtailTransform);
    vtailEntity->addComponent(bodyMaterial);

    m_3dWindow->setRootEntity(m_3dRoot);

    // Keep label above to avoid being covered by createWindowContainer
    m_attLabel = new QLabel(QString::fromUtf8(u8"Roll: 0   Pitch: 0   Yaw: 0"), m_tab3d);
    m_attLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_attLabel->setMinimumHeight(24);
    m_attLabel->setStyleSheet("color: #f0f0f0; background-color: rgba(0,0,0,120); padding:4px; font-weight:600;");

    m_3dContainer = QWidget::createWindowContainer(m_3dWindow, m_tab3d);
    m_3dContainer->setFocusPolicy(Qt::StrongFocus);

    layout->addWidget(m_attLabel, 0);
    layout->addWidget(m_3dContainer, 1);
}

void MainWindow::updateAttitude(double rollDeg, double pitchDeg, double yawDeg)
{
    if (!m_modelTransform) return;
    QQuaternion q = QQuaternion::fromEulerAngles(pitchDeg, yawDeg, rollDeg);
    m_modelTransform->setRotation(q);
    if (m_attLabel) {
        m_attLabel->setText(QStringLiteral("Roll: %1   Pitch: %2   Yaw: %3")
                                .arg(QString::number(rollDeg, 'f', 1))
                                .arg(QString::number(pitchDeg, 'f', 1))
                                .arg(QString::number(yawDeg, 'f', 1)));
    }
}

bool MainWindow::tryParseAttitude(const QString &text, double &roll, double &pitch, double &yaw) const
{
    QString str = text.trimmed();
    if (m_useAttRegex && !m_attRegex.isEmpty()) {
        QRegularExpression re(m_attRegex);
        QRegularExpressionMatch m = re.match(str);
        if (m.hasMatch() && m.lastCapturedIndex() >= 3) {
            bool ok1=false, ok2=false, ok3=false;
            double r = m.captured(1).toDouble(&ok1);
            double p = m.captured(2).toDouble(&ok2);
            double y = m.captured(3).toDouble(&ok3);
            if (ok1 && ok2 && ok3) {
                roll = r; pitch = p; yaw = y;
                return true;
            }
        }
    }
    const QStringList parts = str.split(',', Qt::SkipEmptyParts);
    if (parts.size() != 3) return false;
    bool ok1=false, ok2=false, ok3=false;
    double r = parts[0].toDouble(&ok1);
    double p = parts[1].toDouble(&ok2);
    double y = parts[2].toDouble(&ok3);
    if (!(ok1 && ok2 && ok3)) return false;
    roll = r;
    pitch = p;
    yaw = y;
    return true;
}
