#include "attitudeworker.h"

AttitudeWorker::AttitudeWorker(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(20); // 50 Hz update cap
    connect(m_timer, &QTimer::timeout, this, &AttitudeWorker::flush);
    m_timer->start();
}

void AttitudeWorker::appendAttitude(double rollDeg, double pitchDeg, double yawDeg)
{
    QMutexLocker locker(&m_mutex);
    m_roll = rollDeg;
    m_pitch = pitchDeg;
    m_yaw = yawDeg;
    m_dirty = true;
}

void AttitudeWorker::flush()
{
    double r, p, y;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_dirty) return;
        r = m_roll;
        p = m_pitch;
        y = m_yaw;
        m_dirty = false;
    }
    emit attitudeReady(r, p, y);
}
