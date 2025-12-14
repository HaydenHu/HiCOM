#ifndef SERIALSETTINGS_H
#define SERIALSETTINGS_H

#include <QString>
#include <QSerialPort>

struct SerialSettings {
    QString portName;
    QSerialPort::BaudRate baudRate;
    QSerialPort::DataBits dataBits;
    QSerialPort::Parity parity;
    QSerialPort::StopBits stopBits;
    QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;
    bool dtrEnabled = false;
};

#endif // SERIALSETTINGS_H
