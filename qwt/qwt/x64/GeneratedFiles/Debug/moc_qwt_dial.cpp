/****************************************************************************
** Meta object code from reading C++ file 'qwt_dial.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/qwt_dial.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qwt_dial.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7QwtDialE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN7QwtDialE = QtMocHelpers::stringData(
    "QwtDial",
    "lineWidth",
    "frameShadow",
    "Shadow",
    "mode",
    "Mode",
    "origin",
    "minScaleArc",
    "maxScaleArc",
    "Plain",
    "Raised",
    "Sunken",
    "RotateNeedle",
    "RotateScale"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN7QwtDialE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       6,   14, // properties
       2,   44, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // properties: name, type, flags, notifyId, revision
       1, QMetaType::Int, 0x00015103, uint(-1), 0,
       2, 0x80000000 | 3, 0x0001510b, uint(-1), 0,
       4, 0x80000000 | 5, 0x0001510b, uint(-1), 0,
       6, QMetaType::Double, 0x00015103, uint(-1), 0,
       7, QMetaType::Double, 0x00015103, uint(-1), 0,
       8, QMetaType::Double, 0x00015103, uint(-1), 0,

 // enums: name, alias, flags, count, data
       3,    3, 0x0,    3,   54,
       5,    5, 0x0,    2,   60,

 // enum data: key, value
       9, uint(QwtDial::Plain),
      10, uint(QwtDial::Raised),
      11, uint(QwtDial::Sunken),
      12, uint(QwtDial::RotateNeedle),
      13, uint(QwtDial::RotateScale),

       0        // eod
};

Q_CONSTINIT const QMetaObject QwtDial::staticMetaObject = { {
    QMetaObject::SuperData::link<QwtAbstractSlider::staticMetaObject>(),
    qt_meta_stringdata_ZN7QwtDialE.offsetsAndSizes,
    qt_meta_data_ZN7QwtDialE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN7QwtDialE_t,
        // property 'lineWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'frameShadow'
        QtPrivate::TypeAndForceComplete<Shadow, std::true_type>,
        // property 'mode'
        QtPrivate::TypeAndForceComplete<Mode, std::true_type>,
        // property 'origin'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'minScaleArc'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'maxScaleArc'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // enum 'Shadow'
        QtPrivate::TypeAndForceComplete<QwtDial::Shadow, std::true_type>,
        // enum 'Mode'
        QtPrivate::TypeAndForceComplete<QwtDial::Mode, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QwtDial, std::true_type>
    >,
    nullptr
} };

void QwtDial::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QwtDial *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< int*>(_v) = _t->lineWidth(); break;
        case 1: *reinterpret_cast< Shadow*>(_v) = _t->frameShadow(); break;
        case 2: *reinterpret_cast< Mode*>(_v) = _t->mode(); break;
        case 3: *reinterpret_cast< double*>(_v) = _t->origin(); break;
        case 4: *reinterpret_cast< double*>(_v) = _t->minScaleArc(); break;
        case 5: *reinterpret_cast< double*>(_v) = _t->maxScaleArc(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setLineWidth(*reinterpret_cast< int*>(_v)); break;
        case 1: _t->setFrameShadow(*reinterpret_cast< Shadow*>(_v)); break;
        case 2: _t->setMode(*reinterpret_cast< Mode*>(_v)); break;
        case 3: _t->setOrigin(*reinterpret_cast< double*>(_v)); break;
        case 4: _t->setMinScaleArc(*reinterpret_cast< double*>(_v)); break;
        case 5: _t->setMaxScaleArc(*reinterpret_cast< double*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QwtDial::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QwtDial::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN7QwtDialE.stringdata0))
        return static_cast<void*>(this);
    return QwtAbstractSlider::qt_metacast(_clname);
}

int QwtDial::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QwtAbstractSlider::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    return _id;
}
QT_WARNING_POP
