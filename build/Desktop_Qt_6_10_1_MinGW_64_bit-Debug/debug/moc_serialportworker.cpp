/****************************************************************************
** Meta object code from reading C++ file 'serialportworker.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../serialportworker.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'serialportworker.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN16SerialPortWorkerE_t {};
} // unnamed namespace

template <> constexpr inline auto SerialPortWorker::qt_create_metaobjectdata<qt_meta_tag_ZN16SerialPortWorkerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SerialPortWorker",
        "packetReady",
        "",
        "packet",
        "errorOccurred",
        "err",
        "fatalError",
        "portOpened",
        "portClosed",
        "infoMessage",
        "msg",
        "initializeSerialPort",
        "startPort",
        "SerialSettings",
        "settings",
        "stopPort",
        "writeToPort",
        "data",
        "restartPort",
        "onDataReceived",
        "handleError",
        "QSerialPort::SerialPortError",
        "error",
        "watchdogTimeout",
        "processPackets"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'packetReady'
        QtMocHelpers::SignalData<void(QByteArray)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 3 },
        }}),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(QString)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Signal 'fatalError'
        QtMocHelpers::SignalData<void(QString)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Signal 'portOpened'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'portClosed'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'infoMessage'
        QtMocHelpers::SignalData<void(QString)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 10 },
        }}),
        // Slot 'initializeSerialPort'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'startPort'
        QtMocHelpers::SlotData<void(const SerialSettings &)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
        // Slot 'stopPort'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'writeToPort'
        QtMocHelpers::SlotData<void(const QByteArray &)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 17 },
        }}),
        // Slot 'restartPort'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'onDataReceived'
        QtMocHelpers::SlotData<void()>(19, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'handleError'
        QtMocHelpers::SlotData<void(QSerialPort::SerialPortError)>(20, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 21, 22 },
        }}),
        // Slot 'watchdogTimeout'
        QtMocHelpers::SlotData<void()>(23, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'processPackets'
        QtMocHelpers::SlotData<void()>(24, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SerialPortWorker, qt_meta_tag_ZN16SerialPortWorkerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SerialPortWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16SerialPortWorkerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16SerialPortWorkerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16SerialPortWorkerE_t>.metaTypes,
    nullptr
} };

void SerialPortWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SerialPortWorker *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->packetReady((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 1: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->fatalError((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->portOpened(); break;
        case 4: _t->portClosed(); break;
        case 5: _t->infoMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->initializeSerialPort(); break;
        case 7: _t->startPort((*reinterpret_cast<std::add_pointer_t<SerialSettings>>(_a[1]))); break;
        case 8: _t->stopPort(); break;
        case 9: _t->writeToPort((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 10: _t->restartPort(); break;
        case 11: _t->onDataReceived(); break;
        case 12: _t->handleError((*reinterpret_cast<std::add_pointer_t<QSerialPort::SerialPortError>>(_a[1]))); break;
        case 13: _t->watchdogTimeout(); break;
        case 14: _t->processPackets(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SerialPortWorker::*)(QByteArray )>(_a, &SerialPortWorker::packetReady, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SerialPortWorker::*)(QString )>(_a, &SerialPortWorker::errorOccurred, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (SerialPortWorker::*)(QString )>(_a, &SerialPortWorker::fatalError, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (SerialPortWorker::*)()>(_a, &SerialPortWorker::portOpened, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (SerialPortWorker::*)()>(_a, &SerialPortWorker::portClosed, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (SerialPortWorker::*)(QString )>(_a, &SerialPortWorker::infoMessage, 5))
            return;
    }
}

const QMetaObject *SerialPortWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SerialPortWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16SerialPortWorkerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SerialPortWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 15;
    }
    return _id;
}

// SIGNAL 0
void SerialPortWorker::packetReady(QByteArray _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void SerialPortWorker::errorOccurred(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void SerialPortWorker::fatalError(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void SerialPortWorker::portOpened()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void SerialPortWorker::portClosed()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void SerialPortWorker::infoMessage(QString _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
