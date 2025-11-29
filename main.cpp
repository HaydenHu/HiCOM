#include "mainwindow.h"

#include <QApplication>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    // 高DPI支持
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);

    // 将内置 NotoColorEmoji 加为应用级 fallback，确保缺少系统 emoji 字体时也能显示
    const int emojiId = QFontDatabase::addApplicationFont(":/font/NotoColorEmoji_WindowsCompatible.ttf");
    if (emojiId >= 0) {
        const QString emojiFamily = QFontDatabase::applicationFontFamilies(emojiId).value(0);
        if (!emojiFamily.isEmpty()) {
            QFont f = a.font();
            f.setFamilies({f.family(), emojiFamily});
            a.setFont(f);
        }
    }

    MainWindow w;
    w.setWindowIcon(QIcon(":/logo64.ico"));   //添加程序图标
    w.show();
    return a.exec();
}
