#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "QSerialPortInfo"
#include <QSerialPort>
#include <QMessageBox>
#include <QDateTime>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    QStringList serialNamePort;

    serialPort = new QSerialPort(this);
    connect(serialPort,SIGNAL(readyRead()),this,SLOT(manual_serialPortReadyRead()));/*手动连接槽函数*/

    /*找出当前连接的串口并显示到serialCb*/
    //foreach(const QSerialPortInfo &info,QSerialPortInfo::availablePorts())
    //{
    //serialNamePort<<info.portName();// 自动扫描当前可用串口，返回值追加到字符数组中
    //}
    //ui->serailCb->addItems(serialNamePort);// 可用串口号，显示到串口选择下拉框中
    ui->serialCb->clear();
    //通过QSerialPortInfo查找可用串口
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        ui->serialCb->addItem(info.portName());
    }

    // 发送、接收计数清零
    sendNum = 0;
    recvNum = 0;
    // 状态栏
    QStatusBar *sBar = statusBar();
    // 状态栏的收、发计数标签
    lblSendNum = new QLabel(this);
    lblRecvNum = new QLabel(this);
    lblPortState = new QLabel(this);
    lblPortState->setText("Connected");
    //设置lblPortState左对齐
    lblPortState->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    //设置串口状态标签为绿色 表示已连接状态
    lblPortState->setStyleSheet("color:red");

    // 设置标签最小大小
    lblSendNum->setMinimumSize(100, 20);
    lblRecvNum->setMinimumSize(100, 20);
    lblPortState->setMinimumSize(550, 20);
    setNumOnLabel(lblSendNum, "S: ", sendNum);
    setNumOnLabel(lblRecvNum, "R: ", recvNum);
    // 从右往左依次添加
    sBar->addPermanentWidget(lblPortState);
    sBar->addPermanentWidget(lblSendNum);
    sBar->addPermanentWidget(lblRecvNum);

    // 定时发送-定时器
    timSend = new QTimer;
    timSend->setInterval(1000);// 设置默认定时时长1000ms
    connect(timSend, &QTimer::timeout, this, [=](){
        on_sendBt_clicked();
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

//检测通讯端口槽函数
void MainWindow::on_btnSerialCheck_clicked()
{
    ui->serialCb->clear();
    //通过QSerialPortInfo查找可用串口
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        ui->serialCb->addItem(info.portName());
    }
}

/*手动实现接收数据函数*/
void MainWindow::manual_serialPortReadyRead()
{
    QByteArray recBuf = serialPort->readAll();;
    QString str_rev;

    // 接收字节计数
    recvNum += recBuf.size();
    // 状态栏显示计数值
    setNumOnLabel(lblRecvNum, "R: ", recvNum);

    if(ui->chk_rev_hex->checkState() == false){
        if(ui->chk_rev_time->checkState() == Qt::Checked){
            QDateTime nowtime = QDateTime::currentDateTime();
            str_rev = "[" + nowtime.toString("yyyy-MM-dd hh:mm:ss") + "] ";
            str_rev += QString(recBuf).append("\r\n");
        }
        else{
            // 在当前位置插入文本，不会发生换行。如果没有移动光标到文件结尾，会导致文件超出当前界面显示范围，界面也不会向下滚动。
            //ui->recvEdit->appendPlainText(buf);

            if(ui->chk_rev_line->checkState() == Qt::Checked){
                str_rev = QString(recBuf).append("\r\n");
            }
            else
            {
                str_rev = QString(recBuf);
            }
        }
    }else{

        // 16进制显示，并转换为大写
        QString str1 = recBuf.toHex().toUpper();//.data();
        // 添加空格
        QString str2;
        for(int i = 0; i<str1.length (); i+=2)
        {
            str2 += str1.mid (i,2);
            str2 += " ";
        }
        if(ui->chk_rev_time->checkState() == Qt::Checked)
        {
            QDateTime nowtime = QDateTime::currentDateTime();
            str_rev = "[" + nowtime.toString("yyyy-MM-dd hh:mm:ss") + "] ";
            str_rev += str2.append("\r\n");
        }
        else
        {
            if(ui->chk_rev_line->checkState() == Qt::Checked)
                str_rev += str2.append("\r\n");
            else
                str_rev = str2;

        }
    }
    ui->recvEdit->insertPlainText(str_rev);
    ui->recvEdit->moveCursor(QTextCursor::End);

}

/*打开串口*/
void MainWindow::on_openBt_clicked()
{
    /*串口初始化*/
    QSerialPort::BaudRate baudRate;
    QSerialPort::DataBits dataBits;
    QSerialPort::StopBits stopBits;
    QSerialPort::Parity checkBits;

    // 获取串口波特率
    // baudRate = ui->baundrateCb->currentText().toInt();直接字符串转换为 int 的方法

    if(ui->baundrateCb->currentText()=="1200")
        baudRate=QSerialPort::Baud1200;
    else if(ui->baundrateCb->currentText()=="2400")
        baudRate=QSerialPort::Baud2400;
    else if(ui->baundrateCb->currentText()=="4800")
        baudRate=QSerialPort::Baud4800;
    else if(ui->baundrateCb->currentText()=="9600")
        baudRate=QSerialPort::Baud9600;
    else if(ui->baundrateCb->currentText()=="19200")
        baudRate=QSerialPort::Baud19200;
    else if(ui->baundrateCb->currentText()=="38400")
        baudRate=QSerialPort::Baud38400;
    else if(ui->baundrateCb->currentText()=="57600")
        baudRate=QSerialPort::Baud57600;
    else if(ui->baundrateCb->currentText()=="115200")
        baudRate=QSerialPort::Baud115200;
    else {
        int baudRateInt = ui->baundrateCb->currentText().toInt();
        QSerialPort::BaudRate baudRate = static_cast<QSerialPort::BaudRate>(baudRateInt);
        serialPort->setBaudRate(baudRate);
    }

    // 获取串口数据位
    if(ui->databitCb->currentText()=="5")
        dataBits=QSerialPort::Data5;
    else if(ui->databitCb->currentText()=="6")
        dataBits=QSerialPort::Data6;
    else if(ui->databitCb->currentText()=="7")
        dataBits=QSerialPort::Data7;
    else if(ui->databitCb->currentText()=="8")
        dataBits=QSerialPort::Data8;

    // 获取串口停止位
    if(ui->stopbitCb->currentText()=="1")
        stopBits=QSerialPort::OneStop;
    else if(ui->stopbitCb->currentText()=="1.5")
        stopBits=QSerialPort::OneAndHalfStop;
    else if(ui->stopbitCb->currentText()=="2")
        stopBits=QSerialPort::TwoStop;

    // 获取串口奇偶校验位
    if(ui->checkbitCb->currentText() == "none"){
        checkBits = QSerialPort::NoParity;
    }else if(ui->checkbitCb->currentText() == "奇校验"){
        checkBits = QSerialPort::OddParity;
    }else if(ui->checkbitCb->currentText() == "偶校验"){
        checkBits = QSerialPort::EvenParity;
    }else{

    }

    // 初始化串口属性，设置 端口号、波特率、数据位、停止位、奇偶校验位数
    serialPort->setPortName(ui->serialCb->currentText());
    serialPort->setBaudRate(baudRate);
    serialPort->setDataBits(dataBits);
    serialPort->setStopBits(stopBits);
    serialPort->setParity(checkBits);

    // 根据初始化好的串口属性，打开串口
    // 如果打开成功，反转打开按钮显示和功能。打开失败，无变化，并且弹出错误对话框。
    if(ui->openBt->text() == "打开串口"){
        if(serialPort->open(QIODevice::ReadWrite) == true){
            //QMessageBox::
            ui->openBt->setText("关闭串口");
            // 让端口号下拉框不可选，避免误操作（选择功能不可用，控件背景为灰色）
            ui->serialCb->setEnabled(false);
        }else{
            QMessageBox::critical(this, "错误提示", "串口打开失败！！！\r\n该串口可能被占用\r\n请选择正确的串口");
        }
        //statusBar 状态栏显示端口状态
        QString sm = "%1 OPENED, %2, 8, NONE, 1";
        QString status = sm.arg(serialPort->portName()).arg(serialPort->baudRate());
        lblPortState->setText(status);
        lblPortState->setStyleSheet("color:green");
    }else{
        serialPort->close();
        ui->openBt->setText("打开串口");
        // 端口号下拉框恢复可选，避免误操作
        ui->serialCb->setEnabled(true);
        //statusBar 状态栏显示端口状态
        QString sm = "%1 CLOSED";
        QString status = sm.arg(serialPort->portName());
        lblPortState->setText(status);
        lblPortState->setStyleSheet("color:red");
    }

}

/*发送数据*/
void MainWindow::on_sendBt_clicked()
{
    QByteArray array;

    //Hex复选框
    if(ui->chk_send_hex->checkState() == Qt::Checked){
        //array = QString2Hex(data);  //HEX 16进制
        array = QByteArray::fromHex(ui->sendEdit->toPlainText().toUtf8()).data();
    }else{
        //array = data.toLatin1();    //ASCII
        array = ui->sendEdit->toPlainText().toLocal8Bit().data();
    }

    if(ui->chk_send_line->checkState() == Qt::Checked){
        array.append("\r\n");
    }
    // 如发送成功，会返回发送的字节长度。失败，返回-1。
    int a = serialPort->write(array);
    // 发送字节计数并显示
    if(a > 0)
    {
        // 发送字节计数
        sendNum += a;
        // 状态栏显示计数值
        setNumOnLabel(lblSendNum, "S: ", sendNum);
    }
}
// 状态栏标签显示计数值
void MainWindow::setNumOnLabel(QLabel *lbl, QString strS, long num)
{
    // 标签显示
    QString str = strS + QString::number(num);
    lbl->setText(str);
}
/*清空*/
void MainWindow::on_clearBt_clicked()
{
    ui->recvEdit->clear();
    // 清除发送、接收字节计数
    sendNum = 0;
    recvNum = 0;
    // 状态栏显示计数值
    setNumOnLabel(lblSendNum, "S: ", sendNum);
    setNumOnLabel(lblRecvNum, "R: ", recvNum);
}

void MainWindow::on_btnClearSend_clicked()
{
    ui->sendEdit->clear();
    // 清除发送字节计数
    sendNum = 0;
    // 状态栏显示计数值
    setNumOnLabel(lblSendNum, "S: ", sendNum);
}
// 定时发送开关 选择复选框
void MainWindow::on_chkTimSend_stateChanged(int arg1)
{
    // 获取复选框状态，未选为0，选中为2
    if(arg1 == 0){
        timSend->stop();
        // 时间输入框恢复可选
        ui->txtSendMs->setEnabled(true);
    }else{
        // 对输入的值做限幅，小于10ms会弹出对话框提示
        if(ui->txtSendMs->text().toInt() >= 10){
            timSend->start(ui->txtSendMs->text().toInt());// 设置定时时长，重新计数
            // 让时间输入框不可选，避免误操作（输入功能不可用，控件背景为灰色）
            ui->txtSendMs->setEnabled(false);
        }else{
            ui->chkTimSend->setCheckState(Qt::Unchecked);
            QMessageBox::critical(this, "错误提示", "定时发送的最小间隔为 10ms\r\n请确保输入的值 >=10");
        }
    }
}
