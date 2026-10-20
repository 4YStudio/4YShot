/****************************************************************************
** Meta object code from reading C++ file 'toolbar.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/ui/toolbar.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'toolbar.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9fouryshot2ui10PreviewBoxE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN9fouryshot2ui10PreviewBoxE = QtMocHelpers::stringData(
    "fouryshot::ui::PreviewBox"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN9fouryshot2ui10PreviewBoxE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

       0        // eod
};

Q_CONSTINIT const QMetaObject fouryshot::ui::PreviewBox::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN9fouryshot2ui10PreviewBoxE.offsetsAndSizes,
    qt_meta_data_ZN9fouryshot2ui10PreviewBoxE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN9fouryshot2ui10PreviewBoxE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<PreviewBox, std::true_type>
    >,
    nullptr
} };

void fouryshot::ui::PreviewBox::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PreviewBox *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *fouryshot::ui::PreviewBox::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *fouryshot::ui::PreviewBox::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN9fouryshot2ui10PreviewBoxE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int fouryshot::ui::PreviewBox::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    return _id;
}
namespace {
struct qt_meta_tag_ZN9fouryshot2ui7ToolbarE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN9fouryshot2ui7ToolbarE = QtMocHelpers::stringData(
    "fouryshot::ui::Toolbar",
    "toolSelected",
    "",
    "annotation::ToolType",
    "tool",
    "colorChanged",
    "color",
    "strokeWidthChanged",
    "width",
    "textFontChanged",
    "family",
    "size",
    "filterParamsChanged",
    "isBlur",
    "intensity",
    "stepParamsChanged",
    "startNumber",
    "undoTriggered",
    "redoTriggered",
    "pinTriggered",
    "longCaptureTriggered",
    "ocrTriggered",
    "saveTriggered",
    "copyTriggered",
    "closeTriggered",
    "sizeChanged",
    "onToolButtonClicked",
    "id",
    "onColorButtonClicked"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN9fouryshot2ui7ToolbarE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      17,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      15,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,  116,    2, 0x06,    1 /* Public */,
       5,    1,  119,    2, 0x06,    3 /* Public */,
       7,    1,  122,    2, 0x06,    5 /* Public */,
       9,    2,  125,    2, 0x06,    7 /* Public */,
      12,    2,  130,    2, 0x06,   10 /* Public */,
      15,    2,  135,    2, 0x06,   13 /* Public */,
      17,    0,  140,    2, 0x06,   16 /* Public */,
      18,    0,  141,    2, 0x06,   17 /* Public */,
      19,    0,  142,    2, 0x06,   18 /* Public */,
      20,    0,  143,    2, 0x06,   19 /* Public */,
      21,    0,  144,    2, 0x06,   20 /* Public */,
      22,    0,  145,    2, 0x06,   21 /* Public */,
      23,    0,  146,    2, 0x06,   22 /* Public */,
      24,    0,  147,    2, 0x06,   23 /* Public */,
      25,    0,  148,    2, 0x06,   24 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      26,    1,  149,    2, 0x08,   25 /* Private */,
      28,    1,  152,    2, 0x08,   27 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::QColor,    6,
    QMetaType::Void, QMetaType::Int,    8,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,   10,   11,
    QMetaType::Void, QMetaType::Bool, QMetaType::Int,   13,   14,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   16,   11,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,   27,
    QMetaType::Void, QMetaType::QColor,    6,

       0        // eod
};

Q_CONSTINIT const QMetaObject fouryshot::ui::Toolbar::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN9fouryshot2ui7ToolbarE.offsetsAndSizes,
    qt_meta_data_ZN9fouryshot2ui7ToolbarE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN9fouryshot2ui7ToolbarE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<Toolbar, std::true_type>,
        // method 'toolSelected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<annotation::ToolType, std::false_type>,
        // method 'colorChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QColor &, std::false_type>,
        // method 'strokeWidthChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'textFontChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'filterParamsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'stepParamsChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'undoTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'redoTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'pinTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'longCaptureTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'ocrTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'copyTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'closeTriggered'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'sizeChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onToolButtonClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onColorButtonClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QColor &, std::false_type>
    >,
    nullptr
} };

void fouryshot::ui::Toolbar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Toolbar *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->toolSelected((*reinterpret_cast< std::add_pointer_t<annotation::ToolType>>(_a[1]))); break;
        case 1: _t->colorChanged((*reinterpret_cast< std::add_pointer_t<QColor>>(_a[1]))); break;
        case 2: _t->strokeWidthChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->textFontChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 4: _t->filterParamsChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 5: _t->stepParamsChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 6: _t->undoTriggered(); break;
        case 7: _t->redoTriggered(); break;
        case 8: _t->pinTriggered(); break;
        case 9: _t->longCaptureTriggered(); break;
        case 10: _t->ocrTriggered(); break;
        case 11: _t->saveTriggered(); break;
        case 12: _t->copyTriggered(); break;
        case 13: _t->closeTriggered(); break;
        case 14: _t->sizeChanged(); break;
        case 15: _t->onToolButtonClicked((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 16: _t->onColorButtonClicked((*reinterpret_cast< std::add_pointer_t<QColor>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (Toolbar::*)(annotation::ToolType );
            if (_q_method_type _q_method = &Toolbar::toolSelected; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)(const QColor & );
            if (_q_method_type _q_method = &Toolbar::colorChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)(int );
            if (_q_method_type _q_method = &Toolbar::strokeWidthChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)(const QString & , int );
            if (_q_method_type _q_method = &Toolbar::textFontChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)(bool , int );
            if (_q_method_type _q_method = &Toolbar::filterParamsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)(int , int );
            if (_q_method_type _q_method = &Toolbar::stepParamsChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)();
            if (_q_method_type _q_method = &Toolbar::undoTriggered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)();
            if (_q_method_type _q_method = &Toolbar::redoTriggered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)();
            if (_q_method_type _q_method = &Toolbar::pinTriggered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)();
            if (_q_method_type _q_method = &Toolbar::longCaptureTriggered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 9;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)();
            if (_q_method_type _q_method = &Toolbar::ocrTriggered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 10;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)();
            if (_q_method_type _q_method = &Toolbar::saveTriggered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 11;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)();
            if (_q_method_type _q_method = &Toolbar::copyTriggered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 12;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)();
            if (_q_method_type _q_method = &Toolbar::closeTriggered; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 13;
                return;
            }
        }
        {
            using _q_method_type = void (Toolbar::*)();
            if (_q_method_type _q_method = &Toolbar::sizeChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 14;
                return;
            }
        }
    }
}

const QMetaObject *fouryshot::ui::Toolbar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *fouryshot::ui::Toolbar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN9fouryshot2ui7ToolbarE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int fouryshot::ui::Toolbar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 17;
    }
    return _id;
}

// SIGNAL 0
void fouryshot::ui::Toolbar::toolSelected(annotation::ToolType _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void fouryshot::ui::Toolbar::colorChanged(const QColor & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void fouryshot::ui::Toolbar::strokeWidthChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void fouryshot::ui::Toolbar::textFontChanged(const QString & _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void fouryshot::ui::Toolbar::filterParamsChanged(bool _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void fouryshot::ui::Toolbar::stepParamsChanged(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void fouryshot::ui::Toolbar::undoTriggered()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void fouryshot::ui::Toolbar::redoTriggered()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void fouryshot::ui::Toolbar::pinTriggered()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void fouryshot::ui::Toolbar::longCaptureTriggered()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void fouryshot::ui::Toolbar::ocrTriggered()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void fouryshot::ui::Toolbar::saveTriggered()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void fouryshot::ui::Toolbar::copyTriggered()
{
    QMetaObject::activate(this, &staticMetaObject, 12, nullptr);
}

// SIGNAL 13
void fouryshot::ui::Toolbar::closeTriggered()
{
    QMetaObject::activate(this, &staticMetaObject, 13, nullptr);
}

// SIGNAL 14
void fouryshot::ui::Toolbar::sizeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}
QT_WARNING_POP
