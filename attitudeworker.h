#ifndef ATTITUDEWORKER_H
#define ATTITUDEWORKER_H

#include <QObject>
#include <QMutex>
#include <QTimer>

class AttitudeWorker : public QObject
{
    Q_OBJECT
public:
    explicit AttitudeWorker(QObject *parent = nullptr);

public slots:
    void appendAttitude(double rollDeg, double pitchDeg, double yawDeg);

signals:
    void attitudeReady(double rollDeg, double pitchDeg, double yawDeg);

private slots:
    void flush();

private:
    QMutex m_mutex;
    double m_roll = 0.0;
    double m_pitch = 0.0;
    double m_yaw = 0.0;
    bool m_dirty = false;
    QTimer* m_timer = nullptr;
};

#endif // ATTITUDEWORKER_H
