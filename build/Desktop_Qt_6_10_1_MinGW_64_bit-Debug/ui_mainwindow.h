/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout_9;
    QTabWidget *tabWidget;
    QWidget *tab;
    QHBoxLayout *horizontalLayout_10;
    QVBoxLayout *verticalLayout_5;
    QGroupBox *groupBox;
    QVBoxLayout *verticalLayout_2;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QComboBox *serialCb;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label_2;
    QComboBox *baundrateCb;
    QHBoxLayout *horizontalLayout_3;
    QLabel *label_3;
    QComboBox *databitCb;
    QHBoxLayout *horizontalLayout_4;
    QLabel *label_4;
    QComboBox *stopbitCb;
    QHBoxLayout *horizontalLayout_5;
    QLabel *label_5;
    QComboBox *checkbitCb;
    QPushButton *btnSerialCheck;
    QPushButton *openBt;
    QGroupBox *groupBox_2;
    QHBoxLayout *horizontalLayout_6;
    QPushButton *clearBt;
    QVBoxLayout *verticalLayout;
    QCheckBox *chk_rev_hex;
    QCheckBox *chk_rev_time;
    QCheckBox *chk_rev_line;
    QGroupBox *groupBox_3;
    QHBoxLayout *horizontalLayout_7;
    QVBoxLayout *verticalLayout_4;
    QPushButton *btnClearSend;
    QSpinBox *txtSendMs;
    QVBoxLayout *verticalLayout_3;
    QCheckBox *chk_send_hex;
    QCheckBox *chkTimSend;
    QCheckBox *chk_send_line;
    QVBoxLayout *verticalLayout_6;
    QTextEdit *recvEdit;
    QHBoxLayout *horizontalLayout_8;
    QTextEdit *sendEdit;
    QPushButton *sendBt;
    QWidget *tab_2;
    QVBoxLayout *verticalLayout_7;
    QLabel *label_6;
    QLabel *label_8;
    QLabel *label_7;
    QTextBrowser *textBrowser;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(809, 625);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        horizontalLayout_9 = new QHBoxLayout(centralwidget);
        horizontalLayout_9->setObjectName("horizontalLayout_9");
        tabWidget = new QTabWidget(centralwidget);
        tabWidget->setObjectName("tabWidget");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
        sizePolicy.setHorizontalStretch(100);
        sizePolicy.setVerticalStretch(100);
        sizePolicy.setHeightForWidth(tabWidget->sizePolicy().hasHeightForWidth());
        tabWidget->setSizePolicy(sizePolicy);
        tab = new QWidget();
        tab->setObjectName("tab");
        horizontalLayout_10 = new QHBoxLayout(tab);
        horizontalLayout_10->setObjectName("horizontalLayout_10");
        verticalLayout_5 = new QVBoxLayout();
        verticalLayout_5->setObjectName("verticalLayout_5");
        groupBox = new QGroupBox(tab);
        groupBox->setObjectName("groupBox");
        QSizePolicy sizePolicy1(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(groupBox->sizePolicy().hasHeightForWidth());
        groupBox->setSizePolicy(sizePolicy1);
        verticalLayout_2 = new QVBoxLayout(groupBox);
        verticalLayout_2->setObjectName("verticalLayout_2");
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName("horizontalLayout");
        label = new QLabel(groupBox);
        label->setObjectName("label");

        horizontalLayout->addWidget(label);

        serialCb = new QComboBox(groupBox);
        serialCb->setObjectName("serialCb");

        horizontalLayout->addWidget(serialCb);


        verticalLayout_2->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        label_2 = new QLabel(groupBox);
        label_2->setObjectName("label_2");

        horizontalLayout_2->addWidget(label_2);

        baundrateCb = new QComboBox(groupBox);
        baundrateCb->addItem(QString());
        baundrateCb->addItem(QString());
        baundrateCb->addItem(QString());
        baundrateCb->addItem(QString());
        baundrateCb->addItem(QString());
        baundrateCb->addItem(QString());
        baundrateCb->addItem(QString());
        baundrateCb->setObjectName("baundrateCb");
        baundrateCb->setEditable(true);

        horizontalLayout_2->addWidget(baundrateCb);


        verticalLayout_2->addLayout(horizontalLayout_2);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        label_3 = new QLabel(groupBox);
        label_3->setObjectName("label_3");

        horizontalLayout_3->addWidget(label_3);

        databitCb = new QComboBox(groupBox);
        databitCb->addItem(QString());
        databitCb->addItem(QString());
        databitCb->addItem(QString());
        databitCb->addItem(QString());
        databitCb->setObjectName("databitCb");

        horizontalLayout_3->addWidget(databitCb);


        verticalLayout_2->addLayout(horizontalLayout_3);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName("horizontalLayout_4");
        label_4 = new QLabel(groupBox);
        label_4->setObjectName("label_4");

        horizontalLayout_4->addWidget(label_4);

        stopbitCb = new QComboBox(groupBox);
        stopbitCb->addItem(QString());
        stopbitCb->addItem(QString());
        stopbitCb->addItem(QString());
        stopbitCb->setObjectName("stopbitCb");

        horizontalLayout_4->addWidget(stopbitCb);


        verticalLayout_2->addLayout(horizontalLayout_4);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName("horizontalLayout_5");
        label_5 = new QLabel(groupBox);
        label_5->setObjectName("label_5");

        horizontalLayout_5->addWidget(label_5);

        checkbitCb = new QComboBox(groupBox);
        checkbitCb->addItem(QString());
        checkbitCb->addItem(QString());
        checkbitCb->addItem(QString());
        checkbitCb->setObjectName("checkbitCb");

        horizontalLayout_5->addWidget(checkbitCb);


        verticalLayout_2->addLayout(horizontalLayout_5);

        btnSerialCheck = new QPushButton(groupBox);
        btnSerialCheck->setObjectName("btnSerialCheck");
        btnSerialCheck->setMinimumSize(QSize(0, 30));

        verticalLayout_2->addWidget(btnSerialCheck);

        openBt = new QPushButton(groupBox);
        openBt->setObjectName("openBt");
        QSizePolicy sizePolicy2(QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(openBt->sizePolicy().hasHeightForWidth());
        openBt->setSizePolicy(sizePolicy2);
        openBt->setMinimumSize(QSize(0, 30));
        openBt->setMaximumSize(QSize(16777215, 100));

        verticalLayout_2->addWidget(openBt);


        verticalLayout_5->addWidget(groupBox);

        groupBox_2 = new QGroupBox(tab);
        groupBox_2->setObjectName("groupBox_2");
        horizontalLayout_6 = new QHBoxLayout(groupBox_2);
        horizontalLayout_6->setObjectName("horizontalLayout_6");
        clearBt = new QPushButton(groupBox_2);
        clearBt->setObjectName("clearBt");
        sizePolicy2.setHeightForWidth(clearBt->sizePolicy().hasHeightForWidth());
        clearBt->setSizePolicy(sizePolicy2);
        clearBt->setMaximumSize(QSize(16777215, 50));

        horizontalLayout_6->addWidget(clearBt);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName("verticalLayout");
        chk_rev_hex = new QCheckBox(groupBox_2);
        chk_rev_hex->setObjectName("chk_rev_hex");

        verticalLayout->addWidget(chk_rev_hex);

        chk_rev_time = new QCheckBox(groupBox_2);
        chk_rev_time->setObjectName("chk_rev_time");

        verticalLayout->addWidget(chk_rev_time);

        chk_rev_line = new QCheckBox(groupBox_2);
        chk_rev_line->setObjectName("chk_rev_line");

        verticalLayout->addWidget(chk_rev_line);


        horizontalLayout_6->addLayout(verticalLayout);


        verticalLayout_5->addWidget(groupBox_2);

        groupBox_3 = new QGroupBox(tab);
        groupBox_3->setObjectName("groupBox_3");
        horizontalLayout_7 = new QHBoxLayout(groupBox_3);
        horizontalLayout_7->setObjectName("horizontalLayout_7");
        verticalLayout_4 = new QVBoxLayout();
        verticalLayout_4->setObjectName("verticalLayout_4");
        btnClearSend = new QPushButton(groupBox_3);
        btnClearSend->setObjectName("btnClearSend");
        sizePolicy2.setHeightForWidth(btnClearSend->sizePolicy().hasHeightForWidth());
        btnClearSend->setSizePolicy(sizePolicy2);
        btnClearSend->setMaximumSize(QSize(16777215, 50));

        verticalLayout_4->addWidget(btnClearSend);

        txtSendMs = new QSpinBox(groupBox_3);
        txtSendMs->setObjectName("txtSendMs");
        txtSendMs->setMaximum(360000);

        verticalLayout_4->addWidget(txtSendMs);


        horizontalLayout_7->addLayout(verticalLayout_4);

        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setObjectName("verticalLayout_3");
        chk_send_hex = new QCheckBox(groupBox_3);
        chk_send_hex->setObjectName("chk_send_hex");

        verticalLayout_3->addWidget(chk_send_hex);

        chkTimSend = new QCheckBox(groupBox_3);
        chkTimSend->setObjectName("chkTimSend");

        verticalLayout_3->addWidget(chkTimSend);

        chk_send_line = new QCheckBox(groupBox_3);
        chk_send_line->setObjectName("chk_send_line");

        verticalLayout_3->addWidget(chk_send_line);


        horizontalLayout_7->addLayout(verticalLayout_3);


        verticalLayout_5->addWidget(groupBox_3);

        verticalLayout_5->setStretch(0, 2);
        verticalLayout_5->setStretch(1, 1);
        verticalLayout_5->setStretch(2, 1);

        horizontalLayout_10->addLayout(verticalLayout_5);

        verticalLayout_6 = new QVBoxLayout();
        verticalLayout_6->setSpacing(6);
        verticalLayout_6->setObjectName("verticalLayout_6");
        recvEdit = new QTextEdit(tab);
        recvEdit->setObjectName("recvEdit");

        verticalLayout_6->addWidget(recvEdit);

        horizontalLayout_8 = new QHBoxLayout();
        horizontalLayout_8->setObjectName("horizontalLayout_8");
        sendEdit = new QTextEdit(tab);
        sendEdit->setObjectName("sendEdit");

        horizontalLayout_8->addWidget(sendEdit);

        sendBt = new QPushButton(tab);
        sendBt->setObjectName("sendBt");
        sizePolicy2.setHeightForWidth(sendBt->sizePolicy().hasHeightForWidth());
        sendBt->setSizePolicy(sizePolicy2);
        sendBt->setBaseSize(QSize(0, 0));
        sendBt->setAutoDefault(false);

        horizontalLayout_8->addWidget(sendBt);


        verticalLayout_6->addLayout(horizontalLayout_8);

        verticalLayout_6->setStretch(0, 5);
        verticalLayout_6->setStretch(1, 1);

        horizontalLayout_10->addLayout(verticalLayout_6);

        tabWidget->addTab(tab, QString());
        tab_2 = new QWidget();
        tab_2->setObjectName("tab_2");
        verticalLayout_7 = new QVBoxLayout(tab_2);
        verticalLayout_7->setObjectName("verticalLayout_7");
        label_6 = new QLabel(tab_2);
        label_6->setObjectName("label_6");
        QFont font;
        font.setPointSize(20);
        label_6->setFont(font);

        verticalLayout_7->addWidget(label_6, 0, Qt::AlignmentFlag::AlignHCenter);

        label_8 = new QLabel(tab_2);
        label_8->setObjectName("label_8");

        verticalLayout_7->addWidget(label_8, 0, Qt::AlignmentFlag::AlignHCenter);

        label_7 = new QLabel(tab_2);
        label_7->setObjectName("label_7");

        verticalLayout_7->addWidget(label_7, 0, Qt::AlignmentFlag::AlignHCenter);

        textBrowser = new QTextBrowser(tab_2);
        textBrowser->setObjectName("textBrowser");
        textBrowser->setOpenExternalLinks(true);

        verticalLayout_7->addWidget(textBrowser);

        tabWidget->addTab(tab_2, QString());

        horizontalLayout_9->addWidget(tabWidget);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 809, 20));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        tabWidget->setCurrentIndex(1);
        baundrateCb->setCurrentIndex(6);
        databitCb->setCurrentIndex(3);
        checkbitCb->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "HiCOM", nullptr));
        groupBox->setTitle(QCoreApplication::translate("MainWindow", "\344\270\262\345\217\243\350\256\276\347\275\256", nullptr));
        label->setText(QCoreApplication::translate("MainWindow", "\347\253\257\345\217\243", nullptr));
        label_2->setText(QCoreApplication::translate("MainWindow", "\346\263\242\347\211\271\347\216\207", nullptr));
        baundrateCb->setItemText(0, QCoreApplication::translate("MainWindow", "1200", nullptr));
        baundrateCb->setItemText(1, QCoreApplication::translate("MainWindow", "2400", nullptr));
        baundrateCb->setItemText(2, QCoreApplication::translate("MainWindow", "4800", nullptr));
        baundrateCb->setItemText(3, QCoreApplication::translate("MainWindow", "9600", nullptr));
        baundrateCb->setItemText(4, QCoreApplication::translate("MainWindow", "19200", nullptr));
        baundrateCb->setItemText(5, QCoreApplication::translate("MainWindow", "38400", nullptr));
        baundrateCb->setItemText(6, QCoreApplication::translate("MainWindow", "115200", nullptr));

        label_3->setText(QCoreApplication::translate("MainWindow", "\346\225\260\346\215\256\344\275\215", nullptr));
        databitCb->setItemText(0, QCoreApplication::translate("MainWindow", "5", nullptr));
        databitCb->setItemText(1, QCoreApplication::translate("MainWindow", "6", nullptr));
        databitCb->setItemText(2, QCoreApplication::translate("MainWindow", "7", nullptr));
        databitCb->setItemText(3, QCoreApplication::translate("MainWindow", "8", nullptr));

        label_4->setText(QCoreApplication::translate("MainWindow", "\345\201\234\346\255\242\344\275\215", nullptr));
        stopbitCb->setItemText(0, QCoreApplication::translate("MainWindow", "1", nullptr));
        stopbitCb->setItemText(1, QCoreApplication::translate("MainWindow", "1.5", nullptr));
        stopbitCb->setItemText(2, QCoreApplication::translate("MainWindow", "2", nullptr));

        label_5->setText(QCoreApplication::translate("MainWindow", "\346\240\241\351\252\214\344\275\215", nullptr));
        checkbitCb->setItemText(0, QCoreApplication::translate("MainWindow", "none", nullptr));
        checkbitCb->setItemText(1, QCoreApplication::translate("MainWindow", "\345\245\207\346\240\241\351\252\214", nullptr));
        checkbitCb->setItemText(2, QCoreApplication::translate("MainWindow", "\345\201\266\346\240\241\351\252\214", nullptr));

        btnSerialCheck->setText(QCoreApplication::translate("MainWindow", "\346\211\253\346\217\217\344\270\262\345\217\243", nullptr));
        openBt->setText(QCoreApplication::translate("MainWindow", "\346\211\223\345\274\200\344\270\262\345\217\243", nullptr));
        groupBox_2->setTitle(QCoreApplication::translate("MainWindow", "\346\216\245\346\224\266\350\256\276\347\275\256", nullptr));
        clearBt->setText(QCoreApplication::translate("MainWindow", "\346\270\205\347\251\272", nullptr));
        chk_rev_hex->setText(QCoreApplication::translate("MainWindow", "HEX", nullptr));
        chk_rev_time->setText(QCoreApplication::translate("MainWindow", "\346\227\266\351\227\264\346\210\263", nullptr));
        chk_rev_line->setText(QCoreApplication::translate("MainWindow", "\350\207\252\345\212\250\346\215\242\350\241\214", nullptr));
        groupBox_3->setTitle(QCoreApplication::translate("MainWindow", "\345\217\221\351\200\201\350\256\276\347\275\256", nullptr));
        btnClearSend->setText(QCoreApplication::translate("MainWindow", "\346\270\205\347\251\272", nullptr));
        chk_send_hex->setText(QCoreApplication::translate("MainWindow", "HEX", nullptr));
        chkTimSend->setText(QCoreApplication::translate("MainWindow", "\350\207\252\345\212\250\345\217\221\351\200\201ms", nullptr));
        chk_send_line->setText(QCoreApplication::translate("MainWindow", "\345\217\221\351\200\201\346\226\260\350\241\214", nullptr));
        sendBt->setText(QCoreApplication::translate("MainWindow", "\345\217\221\351\200\201", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab), QCoreApplication::translate("MainWindow", "\344\270\262\345\217\243", nullptr));
        label_6->setText(QCoreApplication::translate("MainWindow", "\345\237\272\344\272\216Qt\344\270\216C++\347\232\204\350\207\252\347\224\250\344\270\262\345\217\243\345\212\251\346\211\213HiCOM", nullptr));
        label_8->setText(QCoreApplication::translate("MainWindow", "\347\211\210\346\234\254\357\274\2321.0", nullptr));
        label_7->setText(QCoreApplication::translate("MainWindow", "\344\275\234\350\200\205\357\274\232Hayden", nullptr));
        textBrowser->setHtml(QCoreApplication::translate("MainWindow", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><meta charset=\"utf-8\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"hr { height: 1px; border-width: 0; }\n"
"li.unchecked::marker { content: \"\\2610\"; }\n"
"li.checked::marker { content: \"\\2612\"; }\n"
"</style></head><body style=\" font-family:'Microsoft YaHei UI'; font-size:9pt; font-weight:400; font-style:normal;\">\n"
"<p align=\"center\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:16pt; font-weight:700;\">	\345\257\271\344\272\216\347\224\265\345\255\220\345\267\245\347\250\213\345\270\210\346\235\245\350\257\264\357\274\214\344\270\262\345\217\243\345\212\251\346\211\213\346\230\257\345\234\250\345\215\225\347\211\207\346\234\272\345\274\200\345\217\221\350\277\207\347\250\213\344\270\255\346\234\200\345\270\270\347\224\250\347"
                        "\232\204\345\267\245\345\205\267\357\274\214\350\200\214\347\216\260\346\234\211\347\232\204\344\270\262\345\217\243\345\212\251\346\211\213\345\220\204\347\247\215\347\224\250\347\235\200\344\270\215\347\210\275\357\274\214\344\272\216\346\230\257\344\276\277\346\234\211\344\272\206\346\255\244\345\267\245\345\205\267\346\273\241\350\266\263\350\207\252\345\267\261\347\232\204\351\234\200\346\261\202\357\274\214\351\234\200\350\246\201\344\273\200\344\271\210\345\212\237\350\203\275\351\203\275\345\217\257\344\273\245\351\200\232\350\277\207\344\277\256\346\224\271\347\250\213\345\272\217\345\256\214\345\226\204</span><span style=\" font-size:16pt;\">\343\200\202</span></p>\n"
"<p align=\"center\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:16pt; font-weight:700;\">\351\202\256\347\256\261:hayhi@qq.com</span></p>\n"
"<p align=\"center\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0p"
                        "x; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:16pt; font-weight:700;\">B\347\253\231:\346\236\201\345\256\242\345\244\247\350\204\221</span></p>\n"
"<p align=\"center\" style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px; font-size:16pt; font-weight:700;\"><br /></p>\n"
"<p align=\"center\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><img src=\":/image/logo128.ico\" /></p>\n"
"<p align=\"center\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">Emoji\350\241\250\346\203\205\345\244\215\345\210\266\347\253\231\357\274\232<a href=\"https://emoji8.com/zh-hans/\"><span style=\" text-decoration: underline; color:#27bf73;\">Emoji\350\241\250\346\203\205\345\244\247\345\205\250\345\217\257\345\244\215\345\210\266 - Emoji8</span></a></p>\n"
"<p align=\"center\" style=\"-"
                        "qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><br /></p>\n"
"<p align=\"center\" style=\" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14pt;\">\345\274\200\346\272\220\344\273\223\345\272\223\357\274\232</span><a href=\"https://github.com/HaydenHu/HiCOM\"><span style=\" text-decoration: underline; color:#27bf73;\">https://github.com/HaydenHu/HiCOM</span></a></p></body></html>", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_2), QCoreApplication::translate("MainWindow", "\345\205\263\344\272\216", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
