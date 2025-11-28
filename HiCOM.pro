QT       += core gui serialport printsupport 3dcore 3drender 3dinput 3dextras 3dlogic

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17 debug
QMAKE_CXXFLAGS += -Wa,-mbig-obj -Wno-deprecated-declarations

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    ringbuffer.cpp \
    serialportworker.cpp \
    qcustomplot/qcustomplot.cpp \
    waveformworker.cpp \
    attitudeworker.cpp

HEADERS += \
    mainwindow.h \
    serialsettings.h \
    ringbuffer.h \
    serialportworker.h \
    qcustomplot/qcustomplot.h \
    waveformworker.h \
    attitudeworker.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc

RC_ICONS = .\image\logo64.ico          # logo.ico是你图片的文件名

# 启用控制台输出以便查看调试信息
CONFIG += console

DEFINES += ENABLE_DEBUG_LOG=0
