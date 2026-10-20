/****************************************************************************
** Meta object code from reading C++ file 'app.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/app.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'app.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.2. It"
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
struct qt_meta_tag_ZN9fouryshot4core3AppE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN9fouryshot4core3AppE = QtMocHelpers::stringData(
    "fouryshot::core::App",
    "startCapture",
    "",
    "pinFromClipboard",
    "onNewIpcConnection",
    "onCaptureFinished",
    "onLongCaptureRequested",
    "firstSegment",
    "globalRegion",
    "dpr",
    "captureNextLongSegment",
    "onLongCaptureFrameReady",
    "onLongCaptureFinishing",
    "onLongCaptureCancelled",
    "onLongCaptureFinished",
    "onLongCapturePanelDestroyed"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN9fouryshot4core3AppE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      11,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   80,    2, 0x0a,    1 /* Public */,
       3,    0,   81,    2, 0x0a,    2 /* Public */,
       4,    0,   82,    2, 0x08,    3 /* Private */,
       5,    0,   83,    2, 0x08,    4 /* Private */,
       6,    3,   84,    2, 0x08,    5 /* Private */,
      10,    0,   91,    2, 0x08,    9 /* Private */,
      11,    0,   92,    2, 0x08,   10 /* Private */,
      12,    0,   93,    2, 0x08,   11 /* Private */,
      13,    0,   94,    2, 0x08,   12 /* Private */,
      14,    0,   95,    2, 0x08,   13 /* Private */,
      15,    0,   96,    2, 0x08,   14 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QImage, QMetaType::QRect, QMetaType::QReal,    7,    8,    9,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject fouryshot::core::App::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN9fouryshot4core3AppE.offsetsAndSizes,
    qt_meta_data_ZN9fouryshot4core3AppE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN9fouryshot4core3AppE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<App, std::true_type>,
        // method 'startCapture'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'pinFromClipboard'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onNewIpcConnection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCaptureFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onLongCaptureRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QImage &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QRect &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        // method 'captureNextLongSegment'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onLongCaptureFrameReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onLongCaptureFinishing'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onLongCaptureCancelled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onLongCaptureFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onLongCapturePanelDestroyed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void fouryshot::core::App::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<App *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->startCapture(); break;
        case 1: _t->pinFromClipboard(); break;
        case 2: _t->onNewIpcConnection(); break;
        case 3: _t->onCaptureFinished(); break;
        case 4: _t->onLongCaptureRequested((*reinterpret_cast< std::add_pointer_t<QImage>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QRect>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qreal>>(_a[3]))); break;
        case 5: _t->captureNextLongSegment(); break;
        case 6: _t->onLongCaptureFrameReady(); break;
        case 7: _t->onLongCaptureFinishing(); break;
        case 8: _t->onLongCaptureCancelled(); break;
        case 9: _t->onLongCaptureFinished(); break;
        case 10: _t->onLongCapturePanelDestroyed(); break;
        default: ;
        }
    }
}

const QMetaObject *fouryshot::core::App::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *fouryshot::core::App::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN9fouryshot4core3AppE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int fouryshot::core::App::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 11;
    }
    return _id;
}
QT_WARNING_POP
