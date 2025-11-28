#ifndef WAVEFORMWORKER_H
#define WAVEFORMWORKER_H

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QMutex>
#include <QTimer>

class WaveformWorker : public QObject
{
    Q_OBJECT
public:
    explicit WaveformWorker(QObject *parent = nullptr);

public slots:
    void appendData(const QByteArray &data);

signals:
    void dataReady(QVector<QPointF> points);

private slots:
    void flush();

private:
    QMutex m_mutex;
    QVector<QPointF> m_buffer;
    double m_x = 0.0;
    QTimer* m_timer = nullptr;
    int m_maxPoints = 20000; // cap pending points to avoid runaway
};

#endif // WAVEFORMWORKER_H
