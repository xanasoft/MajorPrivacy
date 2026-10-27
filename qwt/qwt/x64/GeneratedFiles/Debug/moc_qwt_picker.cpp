/****************************************************************************
** Meta object code from reading C++ file 'qwt_picker.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/qwt_picker.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qwt_picker.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.3. It"
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
struct qt_meta_tag_ZN9QwtPickerE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN9QwtPickerE = QtMocHelpers::stringData(
    "QwtPicker",
    "activated",
    "",
    "on",
    "selected",
    "polygon",
    "appended",
    "pos",
    "moved",
    "removed",
    "changed",
    "selection",
    "setEnabled",
    "isEnabled",
    "resizeMode",
    "ResizeMode",
    "trackerMode",
    "DisplayMode",
    "trackerPen",
    "trackerFont",
    "rubberBand",
    "RubberBand",
    "rubberBandPen",
    "NoRubberBand",
    "HLineRubberBand",
    "VLineRubberBand",
    "CrossRubberBand",
    "RectRubberBand",
    "EllipseRubberBand",
    "PolygonRubberBand",
    "UserRubberBand",
    "AlwaysOff",
    "AlwaysOn",
    "ActiveOnly",
    "Stretch",
    "KeepSize"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN9QwtPickerE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       7,   14, // methods
       7,   77, // properties
       3,  112, // enums/sets
       0,    0, // constructors
       0,       // flags
       6,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   56,    2, 0x06,   11 /* Public */,
       4,    1,   59,    2, 0x06,   13 /* Public */,
       6,    1,   62,    2, 0x06,   15 /* Public */,
       8,    1,   65,    2, 0x06,   17 /* Public */,
       9,    1,   68,    2, 0x06,   19 /* Public */,
      10,    1,   71,    2, 0x06,   21 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      12,    1,   74,    2, 0x0a,   23 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Bool,    3,
    QMetaType::Void, QMetaType::QPolygon,    5,
    QMetaType::Void, QMetaType::QPoint,    7,
    QMetaType::Void, QMetaType::QPoint,    7,
    QMetaType::Void, QMetaType::QPoint,    7,
    QMetaType::Void, QMetaType::QPolygon,   11,

 // slots: parameters
    QMetaType::Void, QMetaType::Bool,    2,

 // properties: name, type, flags, notifyId, revision
      13, QMetaType::Bool, 0x00015003, uint(-1), 0,
      14, 0x80000000 | 15, 0x0001510b, uint(-1), 0,
      16, 0x80000000 | 17, 0x0001510b, uint(-1), 0,
      18, QMetaType::QPen, 0x00015103, uint(-1), 0,
      19, QMetaType::QFont, 0x00015103, uint(-1), 0,
      20, 0x80000000 | 21, 0x0001510b, uint(-1), 0,
      22, QMetaType::QPen, 0x00015103, uint(-1), 0,

 // enums: name, alias, flags, count, data
      21,   21, 0x0,    8,  127,
      17,   17, 0x0,    3,  143,
      15,   15, 0x0,    2,  149,

 // enum data: key, value
      23, uint(QwtPicker::NoRubberBand),
      24, uint(QwtPicker::HLineRubberBand),
      25, uint(QwtPicker::VLineRubberBand),
      26, uint(QwtPicker::CrossRubberBand),
      27, uint(QwtPicker::RectRubberBand),
      28, uint(QwtPicker::EllipseRubberBand),
      29, uint(QwtPicker::PolygonRubberBand),
      30, uint(QwtPicker::UserRubberBand),
      31, uint(QwtPicker::AlwaysOff),
      32, uint(QwtPicker::AlwaysOn),
      33, uint(QwtPicker::ActiveOnly),
      34, uint(QwtPicker::Stretch),
      35, uint(QwtPicker::KeepSize),

       0        // eod
};

Q_CONSTINIT const QMetaObject QwtPicker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN9QwtPickerE.offsetsAndSizes,
    qt_meta_data_ZN9QwtPickerE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN9QwtPickerE_t,
        // property 'isEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'resizeMode'
        QtPrivate::TypeAndForceComplete<ResizeMode, std::true_type>,
        // property 'trackerMode'
        QtPrivate::TypeAndForceComplete<DisplayMode, std::true_type>,
        // property 'trackerPen'
        QtPrivate::TypeAndForceComplete<QPen, std::true_type>,
        // property 'trackerFont'
        QtPrivate::TypeAndForceComplete<QFont, std::true_type>,
        // property 'rubberBand'
        QtPrivate::TypeAndForceComplete<RubberBand, std::true_type>,
        // property 'rubberBandPen'
        QtPrivate::TypeAndForceComplete<QPen, std::true_type>,
        // enum 'RubberBand'
        QtPrivate::TypeAndForceComplete<QwtPicker::RubberBand, std::true_type>,
        // enum 'DisplayMode'
        QtPrivate::TypeAndForceComplete<QwtPicker::DisplayMode, std::true_type>,
        // enum 'ResizeMode'
        QtPrivate::TypeAndForceComplete<QwtPicker::ResizeMode, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QwtPicker, std::true_type>,
        // method 'activated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'selected'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPolygon &, std::false_type>,
        // method 'appended'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPoint &, std::false_type>,
        // method 'moved'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPoint &, std::false_type>,
        // method 'removed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPoint &, std::false_type>,
        // method 'changed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPolygon &, std::false_type>,
        // method 'setEnabled'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>
    >,
    nullptr
} };

void QwtPicker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QwtPicker *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->activated((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 1: _t->selected((*reinterpret_cast< std::add_pointer_t<QPolygon>>(_a[1]))); break;
        case 2: _t->appended((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 3: _t->moved((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 4: _t->removed((*reinterpret_cast< std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 5: _t->changed((*reinterpret_cast< std::add_pointer_t<QPolygon>>(_a[1]))); break;
        case 6: _t->setEnabled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QwtPicker::*)(bool );
            if (_q_method_type _q_method = &QwtPicker::activated; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QwtPicker::*)(const QPolygon & );
            if (_q_method_type _q_method = &QwtPicker::selected; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QwtPicker::*)(const QPoint & );
            if (_q_method_type _q_method = &QwtPicker::appended; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QwtPicker::*)(const QPoint & );
            if (_q_method_type _q_method = &QwtPicker::moved; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (QwtPicker::*)(const QPoint & );
            if (_q_method_type _q_method = &QwtPicker::removed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (QwtPicker::*)(const QPolygon & );
            if (_q_method_type _q_method = &QwtPicker::changed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = _t->isEnabled(); break;
        case 1: *reinterpret_cast< ResizeMode*>(_v) = _t->resizeMode(); break;
        case 2: *reinterpret_cast< DisplayMode*>(_v) = _t->trackerMode(); break;
        case 3: *reinterpret_cast< QPen*>(_v) = _t->trackerPen(); break;
        case 4: *reinterpret_cast< QFont*>(_v) = _t->trackerFont(); break;
        case 5: *reinterpret_cast< RubberBand*>(_v) = _t->rubberBand(); break;
        case 6: *reinterpret_cast< QPen*>(_v) = _t->rubberBandPen(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setEnabled(*reinterpret_cast< bool*>(_v)); break;
        case 1: _t->setResizeMode(*reinterpret_cast< ResizeMode*>(_v)); break;
        case 2: _t->setTrackerMode(*reinterpret_cast< DisplayMode*>(_v)); break;
        case 3: _t->setTrackerPen(*reinterpret_cast< QPen*>(_v)); break;
        case 4: _t->setTrackerFont(*reinterpret_cast< QFont*>(_v)); break;
        case 5: _t->setRubberBand(*reinterpret_cast< RubberBand*>(_v)); break;
        case 6: _t->setRubberBandPen(*reinterpret_cast< QPen*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QwtPicker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QwtPicker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN9QwtPickerE.stringdata0))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "QwtEventPattern"))
        return static_cast< QwtEventPattern*>(this);
    return QObject::qt_metacast(_clname);
}

int QwtPicker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void QwtPicker::activated(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QwtPicker::selected(const QPolygon & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QwtPicker::appended(const QPoint & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QwtPicker::moved(const QPoint & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void QwtPicker::removed(const QPoint & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void QwtPicker::changed(const QPolygon & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}
QT_WARNING_POP
