#ifndef SERIALPORTWORKER_H
#define SERIALPORTWORKER_H

#include <QObject>
#include <QSerialPort>
#include <QMutex>
#include <QDateTime>
#include <QThread>
#include <QTimer>
#include <QScopedPointer>
#include "ringbuffer.h"
#include "serialsettings.h"
#include <memory>

class SerialPortWorker : public QObject {
    Q_OBJECT

public:
    explicit SerialPortWorker(QObject *parent = nullptr);

signals:
    void packetReady(QByteArray packet);
    void errorOccurred(QString err);
    void fatalError(QString err);
    void portOpened();
    void portClosed();
    void infoMessage(QString msg);

public slots:
    void initializeSerialPort();
    void startPort(const SerialSettings &settings);
    void stopPort();
    void writeToPort(const QByteArray &data);
    void restartPort();
    void setDtr(bool enabled);

private slots:
    void onDataReceived();
    void handleError(QSerialPort::SerialPortError error);
    void watchdogTimeout();
    void processPackets();

private:
    QScopedPointer<QSerialPort> m_serial;
    std::unique_ptr<RingBuffer> m_buffer;
    QMutex m_mutex;
    qint64 m_lastActiveTime = 0;

    SerialSettings m_lastSettings;
    bool m_hasSettings = false;

    static constexpr int kProcessIntervalMs = 10;
    static constexpr int kMaxChunkSize = 4096;
    static constexpr int kMaxDrainPerTick = 256 * 1024;
    static constexpr int kWatchdogSilentMs = 5000;

    QTimer* m_watchdogTimer;
    QTimer* m_processTimer;
    bool m_seenActivity = false;

    bool verifyChecksum(const QByteArray &packet);
};

#endif // SERIALPORTWORKER_H
