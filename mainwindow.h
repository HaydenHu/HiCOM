#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QSerialPort>
#include <QMutex>
#include <QList>
#include <QTimer>
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
    
    // 添加缺失的成员变量
    QThread* m_serialThread;
    SerialPortWorker* m_serialWorker;
    
    // 写队列相关成员变量
    QMutex m_queueMutex;
    QList<QByteArray> m_writeQueue;
    bool m_isWriting;
    bool m_isPortOpen = false;
    QTimer* m_sendTimer = nullptr;
    bool m_autoSend = false;

    SerialSettings getCurrentSerialSettings() const;
    void writeData(const QByteArray &data);
    void processWriteQueue();
    QByteArray buildSendPayload(const QString& text) const;
    void appendDebug(const QString& text);
};
#endif // MAINWINDOW_H
