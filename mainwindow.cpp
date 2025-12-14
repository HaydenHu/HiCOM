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
#include <Qt3DExtras/QPlaneMesh>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QDirectionalLight>
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
        m_hasAttData = false;
        m_recvLineBuffer.clear();
        m_lastRecvFlushMs = 0;
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
    // 发送区 HEX 开关：切换时自动转换内容，校验 HEX 有效性
    connect(ui->chk_send_hex, &QCheckBox::toggled, this, [this](bool on) {
        const QString txt = ui->sendEdit->toPlainText();
        const QString trimmed = txt.trimmed();
        const QRegularExpression hexRe(QStringLiteral("^[0-9A-Fa-f\\s]+$"));

        if (on) {
            const bool looksLikeHex = !trimmed.isEmpty() && hexRe.match(trimmed).hasMatch();
            if (looksLikeHex) {
                QString cleaned = trimmed;
                cleaned.remove(QRegularExpression(QStringLiteral("\\s")));
                if (cleaned.size() % 2 != 0) {
                    QSignalBlocker b(ui->chk_send_hex);
                    ui->chk_send_hex->setChecked(false);
                    QMessageBox::warning(this,
                                         QString::fromUtf8(u8"HEX格式无效"),
                                         QString::fromUtf8(u8"HEX字节数为奇数，请补全或调整后再开启。"));
                    return;
                }
                bool ok = false;
                QByteArray data = parseHexString(trimmed, &ok);
                if (!ok || data.isEmpty()) {
                    QSignalBlocker b(ui->chk_send_hex);
                    ui->chk_send_hex->setChecked(false);
                    QMessageBox::warning(this,
                                         QString::fromUtf8(u8"HEX格式无效"),
                                         QString::fromUtf8(u8"当前内容不是有效的HEX字符串，无法进入HEX模式。"));
                    return;
                }
                QString normalized = QString::fromLatin1(data.toHex(' ').toUpper());
                QSignalBlocker b(ui->sendEdit);
                ui->sendEdit->setPlainText(normalized);
                return;
            }
            // 非 HEX 文本：按 UTF-8 转为 HEX
            QString hex = QString::fromLatin1(txt.toUtf8().toHex(' ').toUpper());
            QSignalBlocker b(ui->sendEdit);
            ui->sendEdit->setPlainText(hex);
        } else {
            bool ok = false;
            QByteArray data = parseHexString(trimmed, &ok);
            if (!ok) {
                QSignalBlocker b(ui->chk_send_hex);
                ui->chk_send_hex->setChecked(true);
                QMessageBox::warning(this,
                                     QString::fromUtf8(u8"HEX格式无效"),
                                     QString::fromUtf8(u8"当前内容不是有效的HEX字符串，无法退出HEX模式。"));
                return;
            }
            QString plain = QString::fromUtf8(data);
            QSignalBlocker b(ui->sendEdit);
            ui->sendEdit->setPlainText(plain);
        }
    });
    connect(ui->chkDtrSend, &QCheckBox::toggled, this, [this](bool on) {
        if (m_isPortOpen) {
            QMetaObject::invokeMethod(m_serialWorker, "setDtr",
                                      Qt::QueuedConnection,
                                      Q_ARG(bool, on));
        }
        m_currentSettings.dtrEnabled = on;
    });
    // ANSI 颜色开关
    m_enableAnsiColors = ui->chk_rev_ansi->isChecked();
    connect(ui->chk_rev_ansi, &QCheckBox::toggled, this, [this](bool on) {
        m_enableAnsiColors = on;
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
    m_waveRegexList = {QString::fromUtf8(u8"(-?\\d+(?:\\.\\d+)?)")};
    m_attRegex = QString::fromUtf8(u8"Roll:\\s*([-+]?\\d+(?:\\.\\d+)?)\\s+Pitch:\\s*([-+]?\\d+(?:\\.\\d+)?)\\s+Yaw:\\s*([-+]?\\d+(?:\\.\\d+)?)");
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

    // 流控
    switch (ui->flowCtrlCb->currentIndex()) {
    case 1: settings.flowControl = QSerialPort::HardwareControl; break;
    case 2: settings.flowControl = QSerialPort::SoftwareControl; break;
    default: settings.flowControl = QSerialPort::NoFlowControl; break;
    }
    settings.dtrEnabled = (ui->chkDtrSend && ui->chkDtrSend->isChecked());
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
    const QString decoded = decodeTextSmart(packet);
    const QString raw = decoded.trimmed();
    if (m_useWaveRegex) {
        if (tryParseWaveValues(raw, waveValues) && !waveValues.isEmpty()) {
            updateWaveformValues(waveValues);
        }
    }
    // 当未启用波形正则或未能成功解析时，不再按字节值灌入波形，避免显示三角波

    // 姿态显示只由解析结果更新，避免原始文本闪烁
    if (m_attWorker && m_useAttRegex) {
        double r, p, y;
        if (tryParseAttitude(decoded, r, p, y)) {
            QMetaObject::invokeMethod(m_attWorker, "appendAttitude", Qt::QueuedConnection,
                                      Q_ARG(double, r), Q_ARG(double, p), Q_ARG(double, y));
        }
    }

    m_rxBytes += packet.size();
    updateStatusLabels();
    updateCustomMatchDisplay(raw);

    const qint64 nowMs = QDateTime::currentDateTime().toMSecsSinceEpoch();
    QStringList linesToAppend;
    auto appendLine = [this, &linesToAppend](const QString& seg, bool addBreak) {
        QString line;
        if (ui->chk_rev_time->isChecked()) {
            const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("[HH:mm:ss.zzz] "));
            m_toggleTimestampColor = !m_toggleTimestampColor;
            const QString color = m_toggleTimestampColor ? QStringLiteral("#007aff") : QStringLiteral("#ff6a00");
            line += QStringLiteral("<span style=\"color:%1;\">%2</span> ").arg(color, ts.toHtmlEscaped());
        }
        QString htmlBody = m_enableAnsiColors ? ansiToHtml(seg) : seg.toHtmlEscaped();
        line += htmlBody;
        if (addBreak && ui->chk_rev_line->isChecked()) line += QStringLiteral("<br/>");
        linesToAppend << line;
    };

    if (ui->chk_rev_hex->isChecked()) {
        appendLine(formatAsHex(packet), ui->chk_rev_line->isChecked());
    } else {
        if (!ui->chk_rev_line->isChecked()) {
            // 未勾选自动换行：每包直接输出，不额外换行
            appendLine(decoded, false);
        } else {
            // 勾选自动换行：仅按换行符换行，超时才按当前缓冲输出
            QString combined = m_recvLineBuffer + decoded;
            int startIdx = 0;
            bool hasEol = false;
            for (int i = 0; i < combined.size(); ++i) {
                const QChar c = combined.at(i);
                if (c == QChar('\r') || c == QChar('\n')) {
                    hasEol = true;
                    const bool crlf = (c == QChar('\r') && i + 1 < combined.size() && combined.at(i + 1) == QChar('\n'));
                    QString seg = combined.mid(startIdx, i - startIdx);
                    appendLine(seg, true);
                    if (crlf) ++i;
                    startIdx = i + 1;
                }
            }
            m_recvLineBuffer = combined.mid(startIdx);

            // 无换行时不立即输出，等待后续；但若超时则按当前缓冲输出一行
            const qint64 gap = (m_lastRecvFlushMs > 0) ? (nowMs - m_lastRecvFlushMs) : std::numeric_limits<qint64>::max();
            if (!hasEol && !m_recvLineBuffer.isEmpty() && gap > 300) {
                appendLine(m_recvLineBuffer, true);
                m_recvLineBuffer.clear();
            }
        }
    }

    QScrollBar* vs = ui->recvEdit->verticalScrollBar();
    int restorePos = -1;
    if (vs && !m_recvAutoFollow) restorePos = vs->value();

    m_inRecvAppend = true;
    for (const QString& l : linesToAppend) {
        ui->recvEdit->append(l);
    }
    m_inRecvAppend = false;
    if (!linesToAppend.isEmpty()) {
        m_lastRecvFlushMs = nowMs;
    }

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
    ui->openBt->setText(QString::fromUtf8(u8"打开串口"));

}

void MainWindow::onPortOpened()
{
    m_isPortOpen = true;
    m_recvAutoFollow = true;
    m_inRecvAppend = false;
    m_utf8Decoder = QStringDecoder(QStringDecoder::Utf8);
    m_hasAttData = false;
    m_recvLineBuffer.clear();
    m_lastRecvFlushMs = 0;

    m_lastAttText.clear();
    updateRecvSearchHighlights();
    if (QScrollBar* vs = ui->recvEdit->verticalScrollBar()) {
        vs->setValue(vs->maximum());
    }
    QTextCursor c = ui->recvEdit->textCursor();
    c.movePosition(QTextCursor::End);
    ui->recvEdit->setTextCursor(c);
    ui->recvEdit->ensureCursorVisible();
    ui->openBt->setText(QString::fromUtf8(u8"关闭串口"));
    ui->serialCb->setEnabled(false);
    ui->baundrateCb->setEnabled(false);
    ui->databitCb->setEnabled(false);
    ui->checkbitCb->setEnabled(false);
    ui->stopbitCb->setEnabled(false);
    if (ui->flowCtrlCb) ui->flowCtrlCb->setEnabled(false);
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
    m_hasAttData = false;
    m_recvLineBuffer.clear();
    m_lastRecvFlushMs = 0;
    m_lastAttText.clear();
    updateRecvSearchHighlights();
    ui->openBt->setText(QString::fromUtf8(u8"打开串口"));
    ui->serialCb->setEnabled(true);
    ui->baundrateCb->setEnabled(true);
    ui->databitCb->setEnabled(true);
    ui->checkbitCb->setEnabled(true);
    ui->stopbitCb->setEnabled(true);
    if (ui->flowCtrlCb) ui->flowCtrlCb->setEnabled(true);
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
    if (watched == m_3dContainer || watched == m_3dWindow) {
        if (event->type() == QEvent::MouseButtonPress) {
            if (QMouseEvent* me = static_cast<QMouseEvent*>(event)) {
                if (me->button() == Qt::RightButton) {
                    m_attViewPaused = true;
                    m_attPauseSeq = m_attUpdateSeq;
                    m_attDragging = true;
                    m_attPressPos = me->pos();
                    m_attDragBase = m_hasAttData ? m_lastAttQuat :
                                   (m_modelTransform ? m_modelTransform->rotation() : QQuaternion());
                }
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_attDragging && (static_cast<QMouseEvent*>(event)->buttons() & Qt::RightButton)) {
                QMouseEvent* me = static_cast<QMouseEvent*>(event);
                const QPoint delta = me->pos() - m_attPressPos;
                const float sens = 0.3f;
                const float dRoll = -delta.y() * sens; // 绕X
                const float dYaw = delta.x() * sens;   // 绕Z
                if (m_modelTransform) {
                    QQuaternion inc = QQuaternion::fromEulerAngles(dRoll, 0.0f, dYaw);
                    QQuaternion updated = inc * m_attDragBase;
                    m_modelTransform->setRotation(updated);
                    setAttitudeLabelFromQuat(updated);
                }
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            // 双击左键：手动归零模型姿态
            m_attViewPaused = false;
            m_attDragging = false;
            QQuaternion ident = QQuaternion::fromEulerAngles(0, 0, 0);
            if (m_modelTransform) {
                m_modelTransform->setRotation(ident);
            }
            if (m_3dWindow) {
                if (auto cam = m_3dWindow->camera()) {
                    cam->setPosition(QVector3D(0.0f, 0.0f, 10.0f));
                    cam->setViewCenter(QVector3D(0, 0, 0));
                    cam->setUpVector(QVector3D(0, 1, 0));
                }
            }
            m_lastAttQuat = ident;
            m_lastAttRoll = m_lastAttPitch = m_lastAttYaw = 0.0;
            m_hasAttData = false;
            setAttitudeLabel(0.0, 0.0, 0.0);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (QMouseEvent* me = static_cast<QMouseEvent*>(event)) {
                if (me->button() == Qt::RightButton) {
                    m_attViewPaused = false;
                    m_attDragging = false;
                    // 松开后：若已有匹配到的姿态数据，恢复到最新匹配；否则保留用户拖动结果
                    if (m_hasAttData) {
                        if (m_modelTransform) {
                            m_modelTransform->setRotation(m_lastAttQuat);
                        }
                        setAttitudeLabel(m_lastAttRoll, m_lastAttPitch, m_lastAttYaw);
                        if (m_3dWindow) {
                            if (auto cam = m_3dWindow->camera()) {
                                cam->setPosition(QVector3D(0.0f, 0.0f, 10.0f));
                                cam->setViewCenter(QVector3D(0, 0, 0));
                                cam->setUpVector(QVector3D(0, 1, 0));
                            }
                        }
                    }
                }
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

void MainWindow::setAttitudeLabelFromQuat(const QQuaternion &q)
{
    if (!m_attLabel) return;
    // Qt 返回 (pitch=绕X, yaw=绕Y, roll=绕Z)
    const QVector3D euler = q.toEulerAngles();
    const double rollDeg = euler.x();
    const double pitchDeg = euler.y();
    const double yawDeg = euler.z();
    setAttitudeLabel(rollDeg, pitchDeg, yawDeg);
}

void MainWindow::setAttitudeLabel(double rollDeg, double pitchDeg, double yawDeg)
{
    if (!m_attLabel) return;
    QString text = QStringLiteral("Roll: %1   Pitch: %2   Yaw: %3")
                       .arg(QString::number(rollDeg, 'f', 1))
                       .arg(QString::number(pitchDeg, 'f', 1))
                       .arg(QString::number(yawDeg, 'f', 1));
    if (text != m_lastAttText) {
        m_lastAttText = text;
        m_attLabel->setText(text);
    }
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

QString MainWindow::cssColorForCode(int code) const
{
    switch (code) {
    case 30: return QStringLiteral("#000000");
    case 31: return QStringLiteral("#c62828");
    case 32: return QStringLiteral("#2e7d32");
    case 33: return QStringLiteral("#f9a825");
    case 34: return QStringLiteral("#1565c0");
    case 35: return QStringLiteral("#8e24aa");
    case 36: return QStringLiteral("#00838f");
    case 37: return QStringLiteral("#e0e0e0");
    case 90: return QStringLiteral("#555555");
    case 91: return QStringLiteral("#ef5350");
    case 92: return QStringLiteral("#66bb6a");
    case 93: return QStringLiteral("#ffca28");
    case 94: return QStringLiteral("#42a5f5");
    case 95: return QStringLiteral("#ab47bc");
    case 96: return QStringLiteral("#26c6da");
    case 97: return QStringLiteral("#ffffff");
    default: return QString();
    }
}

QString MainWindow::normalizeAnsiEscapes(const QString &text) const
{
    // 将可见的转义序列如 "\x1b" 或 "\033" 转成真正的 ESC
    QString normalized = text;
    normalized.replace(QStringLiteral("\\x1b"), QString(QChar(0x1b)), Qt::CaseInsensitive);
    normalized.replace(QStringLiteral("\\033"), QString(QChar(0x1b)), Qt::CaseInsensitive);
    normalized.replace(QStringLiteral("\\e"), QString(QChar(0x1b)), Qt::CaseInsensitive);
    return normalized;
}

QString MainWindow::ansiToHtml(const QString &text) const
{
    QString src = normalizeAnsiEscapes(text);
    QString out;
    QString currentFg;
    QString currentBg;
    bool bold = false;

    auto flushSpan = [&](const QString& segment) {
        if (segment.isEmpty()) return;
        QString escaped = segment.toHtmlEscaped();
        if (!currentFg.isEmpty() || !currentBg.isEmpty() || bold) {
            QString style;
            if (!currentFg.isEmpty()) style += QStringLiteral("color:%1;").arg(currentFg);
            if (!currentBg.isEmpty()) style += QStringLiteral("background-color:%1;").arg(currentBg);
            if (bold) style += QStringLiteral("font-weight:bold;");
            out += QStringLiteral("<span style=\"%1\">%2</span>").arg(style, escaped);
        } else {
            out += escaped;
        }
    };

    QString buffer;
    for (int i = 0; i < src.size(); ++i) {
        QChar ch = src.at(i);
        if (ch != QChar(0x1b)) {
            buffer.append(ch);
            continue;
        }
        // Flush pending text
        flushSpan(buffer);
        buffer.clear();

        // Expect '['
        if (i + 1 >= src.size() || src.at(i + 1) != QChar('[')) {
            continue;
        }
        i += 2;
        QString numStr;
        while (i < src.size()) {
            QChar c = src.at(i);
            if (c.isDigit() || c == QChar(';')) {
                numStr.append(c);
                ++i;
                continue;
            }
            if (c == QChar('m')) {
                QStringList parts = numStr.split(';', Qt::KeepEmptyParts);
                if (parts.isEmpty()) parts << "0";
                for (const QString& p : parts) {
                    bool ok = false;
                    int code = p.toInt(&ok);
                    if (!ok) continue;
                    if (code == 0) { // reset
                        currentFg.clear();
                        currentBg.clear();
                        bold = false;
                    } else if (code == 1) { // bold
                        bold = true;
                    } else if ((code >= 30 && code <= 37) || (code >= 90 && code <= 97)) {
                        QString cval = cssColorForCode(code);
                        if (!cval.isEmpty()) currentFg = cval;
                    } else if ((code >= 40 && code <= 47) || (code >= 100 && code <= 107)) {
                        int fgCode = (code >= 100) ? (code - 60) : code - 10;
                        QString cval = cssColorForCode(fgCode);
                        if (!cval.isEmpty()) currentBg = cval;
                    }
                }
                break;
            }
            // unsupported sequence, stop
            break;
        }
    }
    flushSpan(buffer);
    return out;
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
    attEdit->setPlaceholderText(QString::fromUtf8(u8"例：Roll: 0   Pitch: 0   Yaw: 0"));
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
        waveEdit->setPlainText(QString::fromUtf8(u8"(-?\\d+(?:\\.\\d+)?)"));
        attEdit->setText(QString::fromUtf8(u8"([-+]?\\d+(?:\\.\\d+)?)[,\\s]+([-+]?\\d+(?:\\.\\d+)?)[,\\s]+([-+]?\\d+(?:\\.\\d+)?)"));
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
        if (!m_useAttRegex) {
            m_hasAttData = false;
        }
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
    m_3dWindow->defaultFrameGraph()->setClearColor(QColor(24, 28, 32));
    m_3dRoot = new Qt3DCore::QEntity;

    auto cam = m_3dWindow->camera();
    cam->lens()->setPerspectiveProjection(45.0f, 16.0f/9.0f, 0.1f, 1000.0f);
    cam->setPosition(QVector3D(0.0f, 0.0f, 10.0f));
    cam->setViewCenter(QVector3D(0, 0, 0));

    // 允许鼠标绕原点旋转观察（锁定平移/缩放）
    auto camController = new Qt3DExtras::QOrbitCameraController(m_3dRoot);
    camController->setCamera(cam);
    camController->setLinearSpeed(0.0f); // 禁止平移
    camController->setLookSpeed(90.0f);

    // Brighter directional light
    {
        auto lightEntity = new Qt3DCore::QEntity(m_3dRoot);
        auto dirLight = new Qt3DRender::QDirectionalLight(lightEntity);
        dirLight->setColor(QColor(255, 255, 255));
        dirLight->setIntensity(1.5f);
        dirLight->setWorldDirection(QVector3D(-0.3f, -0.5f, -1.0f));
        lightEntity->addComponent(dirLight);
    }

    // 立方体主体
    m_modelEntity = new Qt3DCore::QEntity(m_3dRoot);
    m_modelTransform = new Qt3DCore::QTransform;
    m_modelTransform->setRotation(QQuaternion::fromEulerAngles(0, 0, 0));
    m_modelTransform->setTranslation(QVector3D(0, 0, 0));
    m_modelEntity->addComponent(m_modelTransform);

    const float half = 1.0f;
    auto baseCube = new Qt3DCore::QEntity(m_modelEntity);
    auto baseMesh = new Qt3DExtras::QCuboidMesh;
    baseMesh->setXExtent(half * 2.0f);
    baseMesh->setYExtent(half * 2.0f);
    baseMesh->setZExtent(half * 2.0f);
    auto baseMat = new Qt3DExtras::QPhongMaterial;
    baseMat->setDiffuse(QColor(90, 105, 130));   // darker body for contrast
    baseMat->setAmbient(QColor(70, 80, 100));
    baseMat->setSpecular(QColor(180, 180, 190));
    baseMat->setShininess(80.0f);
    baseCube->addComponent(baseMesh);
    baseCube->addComponent(baseMat);

    // XYZ 参考轴（红绿蓝）
    auto addAxis = [&](const QVector3D &dir, const QColor &color) {
        // 默认圆柱/圆锥轴沿 +Y，显式旋转到目标方向
        // 圆柱长度等于立方体边长：从中心到另一侧面外
        const float shaftLen = half * 2.0f;   // 立方体边长
        const float shaftRadius = 0.06f;
        const float headLen = 0.35f;
        const float headRadius = 0.12f;
        QQuaternion rot = QQuaternion::rotationTo(QVector3D(0, 1, 0), dir.normalized());

        auto shaft = new Qt3DCore::QEntity(m_modelEntity);
        auto shaftMesh = new Qt3DExtras::QCylinderMesh;
        shaftMesh->setRadius(shaftRadius);
        shaftMesh->setLength(shaftLen);
        auto shaftMat = new Qt3DExtras::QPhongMaterial;
        shaftMat->setDiffuse(color);
        shaftMat->setAmbient(color.darker(120));
        auto shaftTx = new Qt3DCore::QTransform;
        shaftTx->setRotation(rot);
        shaftTx->setTranslation(dir.normalized() * (shaftLen * 0.5f));
        shaft->addComponent(shaftMesh);
        shaft->addComponent(shaftMat);
        shaft->addComponent(shaftTx);

        auto head = new Qt3DCore::QEntity(m_modelEntity);
        auto headMesh = new Qt3DExtras::QConeMesh;
        headMesh->setLength(headLen);
        headMesh->setTopRadius(0.0f);
        headMesh->setBottomRadius(headRadius);
        auto headMat = new Qt3DExtras::QPhongMaterial;
        headMat->setDiffuse(color);
        headMat->setAmbient(color.darker(120));
        auto headTx = new Qt3DCore::QTransform;
        headTx->setRotation(rot);
        // 箭头从圆柱末端再往外延伸半个边长
        headTx->setTranslation(dir.normalized() * (shaftLen + headLen * 0.5f));
        head->addComponent(headMesh);
        head->addComponent(headMat);
        head->addComponent(headTx);
    };
    addAxis(QVector3D(1, 0, 0), QColor(220, 70, 70));   // X
    addAxis(QVector3D(0, 1, 0), QColor(70, 200, 70));   // Y
    addAxis(QVector3D(0, 0, 1), QColor(70, 140, 220));  // Z

    m_3dWindow->setRootEntity(m_3dRoot);

    m_3dWindow->setRootEntity(m_3dRoot);

    // Keep label above to avoid being covered by createWindowContainer
    m_attLabel = new QLabel(QString::fromUtf8(u8"Roll: 0   Pitch: 0   Yaw: 0"), m_tab3d);
    m_attLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_attLabel->setMinimumHeight(24);
    m_attLabel->setStyleSheet("color: #f0f0f0; background-color: rgba(0,0,0,120); padding:4px; font-weight:600;");

    m_3dContainer = QWidget::createWindowContainer(m_3dWindow, m_tab3d);
    m_3dContainer->setFocusPolicy(Qt::StrongFocus);
    m_3dContainer->installEventFilter(this);
    m_3dWindow->installEventFilter(this);

    layout->addWidget(m_attLabel, 0);
    layout->addWidget(m_3dContainer, 1);
}

void MainWindow::updateAttitude(double rollDeg, double pitchDeg, double yawDeg)
{
    if (!m_useAttRegex) return; // 正则关闭时忽略后续姿态更新
    if (!m_modelTransform) return;
    // Qt 定义：fromEulerAngles(pitch=绕X, yaw=绕Y, roll=绕Z)
    // 用户语义：roll=绕X, pitch=绕Y, yaw=绕Z => 直接按 (roll, pitch, yaw) 映射到 (pitch, yaw, roll)
    QQuaternion q = QQuaternion::fromEulerAngles(rollDeg, pitchDeg, yawDeg);
    m_lastAttQuat = q;
    m_lastAttRoll = rollDeg;
    m_lastAttPitch = pitchDeg;
    m_lastAttYaw = yawDeg;
    m_hasAttData = true;
    ++m_attUpdateSeq;
    if (!m_attViewPaused) {
        m_modelTransform->setRotation(q);
        setAttitudeLabel(rollDeg, pitchDeg, yawDeg);
    }
}

bool MainWindow::tryParseAttitude(const QString &text, double &roll, double &pitch, double &yaw) const
{
    if (!m_useAttRegex) return false;
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
