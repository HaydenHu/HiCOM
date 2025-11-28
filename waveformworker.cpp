#include "waveformworker.h"

WaveformWorker::WaveformWorker(QObject *parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(30);
    connect(m_timer, &QTimer::timeout, this, &WaveformWorker::flush);
    m_timer->start();
}

void WaveformWorker::appendData(const QByteArray &data)
{
    if (data.isEmpty()) return;

    QMutexLocker locker(&m_mutex);
    m_buffer.reserve(m_buffer.size() + data.size());
    for (unsigned char ch : data) {
        m_buffer.append(QPointF(m_x, static_cast<double>(ch)));
        m_x += 1.0;
    }
    if (m_buffer.size() > m_maxPoints) {
        const int drop = m_buffer.size() - m_maxPoints;
        m_buffer.erase(m_buffer.begin(), m_buffer.begin() + drop);
    }
}

void WaveformWorker::flush()
{
    QVector<QPointF> copy;
    {
        QMutexLocker locker(&m_mutex);
        if (m_buffer.isEmpty()) return;
        copy = std::move(m_buffer);
        m_buffer.clear();
    }
    emit dataReady(copy);
}
