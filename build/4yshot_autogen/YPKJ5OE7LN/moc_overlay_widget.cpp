/****************************************************************************
** Meta object code from reading C++ file 'overlay_widget.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/overlay_widget.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'overlay_widget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9fouryshot2ui13OverlayWidgetE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN9fouryshot2ui13OverlayWidgetE = QtMocHelpers::stringData(
    "fouryshot::ui::OverlayWidget",
    "captureCompleted",
    "",
    "captureCancelled",
    "longCaptureRequested",
    "firstSegment",
    "globalRegion",
    "dpr",
    "onSelectionChanged",
    "rect",
    "onSelectionStateChanged",
    "SelectionState",
    "state",
    "onToolSelected",
    "annotation::ToolType",
    "tool",
    "onColorChanged",
    "color",
    "onStrokeWidthChanged",
    "width",
    "onTextFontChanged",
    "family",
    "size",
    "onFilterParamsChanged",
    "isBlur",
    "intensity",
    "onStepParamsChanged",
    "startNumber",
    "onUndo",
    "onRedo",
    "onPin",
    "onLongCapture",
    "onOcr",
    "onSave",
    "onCopy",
    "onClose",
    "onCommitText"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN9fouryshot2ui13OverlayWidgetE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      20,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,  134,    2, 0x06,    1 /* Public */,
       3,    0,  135,    2, 0x06,    2 /* Public */,
       4,    3,  136,    2, 0x06,    3 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       8,    1,  143,    2, 0x08,    7 /* Private */,
      10,    1,  146,    2, 0x08,    9 /* Private */,
      13,    1,  149,    2, 0x08,   11 /* Private */,
      16,    1,  152,    2, 0x08,   13 /* Private */,
      18,    1,  155,    2, 0x08,   15 /* Private */,
      20,    2,  158,    2, 0x08,   17 /* Private */,
      23,    2,  163,    2, 0x08,   20 /* Private */,
      26,    2,  168,    2, 0x08,   23 /* Private */,
      28,    0,  173,    2, 0x08,   26 /* Private */,
      29,    0,  174,    2, 0x08,   27 /* Private */,
      30,    0,  175,    2, 0x08,   28 /* Private */,
      31,    0,  176,    2, 0x08,   29 /* Private */,
      32,    0,  177,    2, 0x08,   30 /* Private */,
      33,    0,  178,    2, 0x08,   31 /* Private */,
      34,    0,  179,    2, 0x08,   32 /* Private */,
      35,    0,  180,    2, 0x08,   33 /* Private */,
      36,    0,  181,    2, 0x08,   34 /* Private */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QImage, QMetaType::QRect, QMetaType::QReal,    5,    6,    7,

 // slots: parameters
    QMetaType::Void, QMetaType::QRect,    9,
    QMetaType::Void, 0x80000000 | 11,   12,
    QMetaType::Void, 0x80000000 | 14,   15,
    QMetaType::Void, QMetaType::QColor,   17,
    QMetaType::Void, QMetaType::Int,   19,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   21,   22,
    QMetaType::Void, QMetaType::Bool, QMetaType::Int,   24,   25,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   27,   22,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject fouryshot::ui::OverlayWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN9fouryshot2ui13OverlayWidgetE.offsetsAndSizes,
    qt_meta_data_ZN9fouryshot2ui13OverlayWidgetE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN9fouryshot2ui13OverlayWidgetE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<OverlayWidget, std::true_type>,
        // method 'captureCompleted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'captureCancelled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'longCaptureRequested'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QImage &, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QRect &, std::false_type>,
        QtPrivate::TypeAndForceComplete<qreal, std::false_type>,
        // method 'onSelectionChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QRect &, std::false_type>,
        // method 'onSelectionStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<SelectionState, std::false_type>,
        // method 'onToolSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<annotation::ToolType, std::false_type>,
        // method 'onColorChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QColor &, std::false_type>,
        // method 'onStrokeWidthChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onTextFontChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onFilterParamsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onStepParamsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onUndo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onRedo'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onPin'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onLongCapture'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onOcr'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onSave'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCopy'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onClose'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onCommitText'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void fouryshot::ui::OverlayWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<OverlayWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->captureCompleted(); break;
        case 1: _t->captureCancelled(); break;
        case 2: _t->longCaptureRequested((*reinterpret_cast< std::add_pointer_t<QImage>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QRect>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<qreal>>(_a[3]))); break;
        case 3: _t->onSelectionChanged((*reinterpret_cast< std::add_pointer_t<QRect>>(_a[1]))); break;
        case 4: _t->onSelectionStateChanged((*reinterpret_cast< std::add_pointer_t<SelectionState>>(_a[1]))); break;
        case 5: _t->onToolSelected((*reinterpret_cast< std::add_pointer_t<annotation::ToolType>>(_a[1]))); break;
        case 6: _t->onColorChanged((*reinterpret_cast< std::add_pointer_t<QColor>>(_a[1]))); break;
        case 7: _t->onStrokeWidthChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->onTextFontChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 9: _t->onFilterParamsChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 10: _t->onStepParamsChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 11: _t->onUndo(); break;
        case 12: _t->onRedo(); break;
        case 13: _t->onPin(); break;
        case 14: _t->onLongCapture(); break;
        case 15: _t->onOcr(); break;
        case 16: _t->onSave(); break;
        case 17: _t->onCopy(); break;
        case 18: _t->onClose(); break;
        case 19: _t->onCommitText(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (OverlayWidget::*)();
            if (_q_method_type _q_method = &OverlayWidget::captureCompleted; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (OverlayWidget::*)();
            if (_q_method_type _q_method = &OverlayWidget::captureCancelled; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (OverlayWidget::*)(const QImage & , const QRect & , qreal );
            if (_q_method_type _q_method = &OverlayWidget::longCaptureRequested; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
    }
}

const QMetaObject *fouryshot::ui::OverlayWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *fouryshot::ui::OverlayWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN9fouryshot2ui13OverlayWidgetE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int fouryshot::ui::OverlayWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 20)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 20;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 20)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 20;
    }
    return _id;
}

// SIGNAL 0
void fouryshot::ui::OverlayWidget::captureCompleted()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void fouryshot::ui::OverlayWidget::captureCancelled()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void fouryshot::ui::OverlayWidget::longCaptureRequested(const QImage & _t1, const QRect & _t2, qreal _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_WARNING_POP
