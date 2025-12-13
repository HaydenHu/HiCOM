#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QSerialPort>
#include <QMutex>
#include <QList>
#include <QTimer>
#include <QFile>
#include <QFileDialog>
#include <QEvent>
#include <QWheelEvent>
#include <QLabel>
#include <QStringList>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QToolButton>
#include <QShortcut>
#include <QStringDecoder>
#include <QPoint>
#include <Qt3DCore/QEntity>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QCuboidMesh>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QFirstPersonCameraController>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QConeMesh>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/QTorusMesh>
#include "attitudeworker.h"
#include "qcustomplot/qcustomplot.h"
#include "waveformworker.h"
#include "serialportworker.h"
#include "serialsettings.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void on_openButton_clicked();
    void on_sendButton_clicked();
    void onPacketReceived(const QByteArray &packet);
    void onErrorOccurred(const QString &error);
    void onFatalError(const QString &error);
    void onPortOpened();
    void onPortClosed();
    
private:
    Ui::MainWindow *ui;

    QThread* m_serialThread;
    SerialPortWorker* m_serialWorker;

    QMutex m_queueMutex;
    QList<QByteArray> m_writeQueue;
    bool m_isWriting;
    bool m_isPortOpen = false;
    QTimer* m_sendTimer = nullptr;
    bool m_autoSend = false;
    QTimer* m_portPollTimer = nullptr;
    QLabel* m_statusConn = nullptr;
    QLabel* m_statusRx = nullptr;
    QLabel* m_statusTx = nullptr;
    QLabel* m_statusMatch = nullptr;
    QCustomPlot* m_wavePlot = nullptr;
    QCPGraph* m_waveGraph = nullptr;
    QVector<QCPGraphData> m_waveData;
    int m_waveMaxPoints = 3000;
    QThread* m_waveThread = nullptr;
    WaveformWorker* m_waveWorker = nullptr;
    QWidget* m_tab3d = nullptr;
    Qt3DExtras::Qt3DWindow* m_3dWindow = nullptr;
    QWidget* m_3dContainer = nullptr;
    Qt3DCore::QEntity* m_3dRoot = nullptr;
    Qt3DCore::QTransform* m_modelTransform = nullptr;
    Qt3DCore::QEntity* m_modelEntity = nullptr;
    QThread* m_attThread = nullptr;
    AttitudeWorker* m_attWorker = nullptr;
    QLabel* m_attLabel = nullptr;
    bool m_waveAutoFollow = true;
    bool m_waveRangeUpdating = false;
    double m_waveViewWidth = 300.0; // 展示区只看最近300个采样，避免挤在一起
    double m_waveX = 0.0;
    qint64 m_rxBytes = 0;
    qint64 m_txBytes = 0;
    QStringList m_knownPorts;
    SerialSettings m_currentSettings;
    bool m_hasCurrentSettings = false;
    bool m_enableDebug = true;
    bool m_toggleTimestampColor = false;
    int m_recvFontPt = 0;
    int m_sendFontPt = 0;
    QStringList m_waveRegexList;
    QString m_attRegex;
    QStringList m_customRegexList;
    QString m_customRegexEnableSpec;
    bool m_useWaveRegex = false;
    bool m_useAttRegex = false;
    QToolButton* m_formatBtn = nullptr;
    bool m_recvAutoFollow = true;
    bool m_inRecvAppend = false;
    int m_recvColorToken = 0;
    QWidget* m_recvSearchPanel = nullptr;
    QLineEdit* m_recvSearchEdit = nullptr;
    QToolButton* m_recvSearchClose = nullptr;
    QToolButton* m_recvSearchNext = nullptr;
    QToolButton* m_recvSearchPrev = nullptr;
    mutable QStringDecoder m_utf8Decoder{QStringDecoder::Utf8};
    QString m_lastAttText;
    QQuaternion m_lastAttQuat{QQuaternion::fromEulerAngles(0, 0, 0)};
    bool m_attViewPaused = false;
    bool m_hasAttData = false;
    bool m_attDragging = false;
    QPoint m_attPressPos;
    QQuaternion m_attDragBase;
    double m_lastAttRoll = 0.0;
    double m_lastAttPitch = 0.0;
    double m_lastAttYaw = 0.0;
    int m_attUpdateSeq = 0;
    int m_attPauseSeq = 0;

    SerialSettings getCurrentSerialSettings() const;
    void writeData(const QByteArray &data);
    void processWriteQueue();
    QByteArray buildSendPayload(const QString& text) const;
    void appendDebug(const QString& text);
    void refreshSerialPorts();
    void updateSerialTooltip();
    void updateStatusLabels();
    void checkPortHotplug();
    void saveLogs();
    QString decodeTextSmart(const QByteArray& data) const;
    void setupWaveformTab();
    void updateWaveform(const QVector<QPointF>& points);
    void updateWaveformValues(const QVector<double>& values);
    void setup3DTab();
    void updateAttitude(double rollDeg, double pitchDeg, double yawDeg);
    void setAttitudeLabelFromQuat(const QQuaternion& q);
    void setAttitudeLabel(double rollDeg, double pitchDeg, double yawDeg);
    bool tryParseAttitude(const QString& text, double &roll, double &pitch, double &yaw) const;
    bool tryParseWaveValues(const QString &text, QVector<double> &values) const;
    void updateCustomMatchDisplay(const QString &text);
    QVector<int> parseIndexSpec(const QString &spec, int maxCount) const;
    void openFormatDialog();
    void showRecvSearch();
    void hideRecvSearch();
    void updateRecvSearchHighlights();
    void findInRecv(bool backward);
    QString ansiToHtml(const QString& text) const;
    QString cssColorForCode(int code) const;
    bool m_enableAnsiColors = false;
    QString normalizeAnsiEscapes(const QString& text) const;
    QString m_recvLineBuffer;
    qint64 m_lastRecvFlushMs = 0;
};
#endif // MAINWINDOW_H
