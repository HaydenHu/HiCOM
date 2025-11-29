#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    // 高DPI支持
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowIcon(QIcon(":/logo64.ico"));   //添加程序图标
    w.show();
    return a.exec();
}
