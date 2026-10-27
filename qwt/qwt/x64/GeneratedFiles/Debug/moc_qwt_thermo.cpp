/****************************************************************************
** Meta object code from reading C++ file 'qwt_thermo.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/qwt_thermo.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qwt_thermo.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9QwtThermoE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN9QwtThermoE = QtMocHelpers::stringData(
    "QwtThermo",
    "setValue",
    "",
    "orientation",
    "Qt::Orientation",
    "scalePosition",
    "ScalePosition",
    "originMode",
    "OriginMode",
    "alarmEnabled",
    "alarmLevel",
    "origin",
    "spacing",
    "borderWidth",
    "pipeWidth",
    "value",
    "NoScale",
    "LeadingScale",
    "TrailingScale",
    "OriginMinimum",
    "OriginMaximum",
    "OriginCustom"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN9QwtThermoE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
      10,   23, // properties
       2,   73, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   20,    2, 0x0a,   13 /* Public */,

 // slots: parameters
    QMetaType::Void, QMetaType::Double,    2,

 // properties: name, type, flags, notifyId, revision
       3, 0x80000000 | 4, 0x0001510b, uint(-1), 0,
       5, 0x80000000 | 6, 0x0001510b, uint(-1), 0,
       7, 0x80000000 | 8, 0x0001510b, uint(-1), 0,
       9, QMetaType::Bool, 0x00015103, uint(-1), 0,
      10, QMetaType::Double, 0x00015103, uint(-1), 0,
      11, QMetaType::Double, 0x00015103, uint(-1), 0,
      12, QMetaType::Int, 0x00015103, uint(-1), 0,
      13, QMetaType::Int, 0x00015103, uint(-1), 0,
      14, QMetaType::Int, 0x00015103, uint(-1), 0,
      15, QMetaType::Double, 0x00115103, uint(-1), 0,

 // enums: name, alias, flags, count, data
       6,    6, 0x0,    3,   83,
       8,    8, 0x0,    3,   89,

 // enum data: key, value
      16, uint(QwtThermo::NoScale),
      17, uint(QwtThermo::LeadingScale),
      18, uint(QwtThermo::TrailingScale),
      19, uint(QwtThermo::OriginMinimum),
      20, uint(QwtThermo::OriginMaximum),
      21, uint(QwtThermo::OriginCustom),

       0        // eod
};

Q_CONSTINIT const QMetaObject QwtThermo::staticMetaObject = { {
    QMetaObject::SuperData::link<QwtAbstractScale::staticMetaObject>(),
    qt_meta_stringdata_ZN9QwtThermoE.offsetsAndSizes,
    qt_meta_data_ZN9QwtThermoE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN9QwtThermoE_t,
        // property 'orientation'
        QtPrivate::TypeAndForceComplete<Qt::Orientation, std::true_type>,
        // property 'scalePosition'
        QtPrivate::TypeAndForceComplete<ScalePosition, std::true_type>,
        // property 'originMode'
        QtPrivate::TypeAndForceComplete<OriginMode, std::true_type>,
        // property 'alarmEnabled'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'alarmLevel'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'origin'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'spacing'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'borderWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'pipeWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'value'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // enum 'ScalePosition'
        QtPrivate::TypeAndForceComplete<QwtThermo::ScalePosition, std::true_type>,
        // enum 'OriginMode'
        QtPrivate::TypeAndForceComplete<QwtThermo::OriginMode, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QwtThermo, std::true_type>,
        // method 'setValue'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>
    >,
    nullptr
} };

void QwtThermo::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QwtThermo *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->setValue((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< Qt::Orientation*>(_v) = _t->orientation(); break;
        case 1: *reinterpret_cast< ScalePosition*>(_v) = _t->scalePosition(); break;
        case 2: *reinterpret_cast< OriginMode*>(_v) = _t->originMode(); break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->alarmEnabled(); break;
        case 4: *reinterpret_cast< double*>(_v) = _t->alarmLevel(); break;
        case 5: *reinterpret_cast< double*>(_v) = _t->origin(); break;
        case 6: *reinterpret_cast< int*>(_v) = _t->spacing(); break;
        case 7: *reinterpret_cast< int*>(_v) = _t->borderWidth(); break;
        case 8: *reinterpret_cast< int*>(_v) = _t->pipeWidth(); break;
        case 9: *reinterpret_cast< double*>(_v) = _t->value(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setOrientation(*reinterpret_cast< Qt::Orientation*>(_v)); break;
        case 1: _t->setScalePosition(*reinterpret_cast< ScalePosition*>(_v)); break;
        case 2: _t->setOriginMode(*reinterpret_cast< OriginMode*>(_v)); break;
        case 3: _t->setAlarmEnabled(*reinterpret_cast< bool*>(_v)); break;
        case 4: _t->setAlarmLevel(*reinterpret_cast< double*>(_v)); break;
        case 5: _t->setOrigin(*reinterpret_cast< double*>(_v)); break;
        case 6: _t->setSpacing(*reinterpret_cast< int*>(_v)); break;
        case 7: _t->setBorderWidth(*reinterpret_cast< int*>(_v)); break;
        case 8: _t->setPipeWidth(*reinterpret_cast< int*>(_v)); break;
        case 9: _t->setValue(*reinterpret_cast< double*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QwtThermo::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QwtThermo::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN9QwtThermoE.stringdata0))
        return static_cast<void*>(this);
    return QwtAbstractScale::qt_metacast(_clname);
}

int QwtThermo::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QwtAbstractScale::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 1;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    return _id;
}
QT_WARNING_POP
