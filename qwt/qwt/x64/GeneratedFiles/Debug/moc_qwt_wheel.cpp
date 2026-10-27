/****************************************************************************
** Meta object code from reading C++ file 'qwt_wheel.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/qwt_wheel.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qwt_wheel.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN8QwtWheelE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN8QwtWheelE = QtMocHelpers::stringData(
    "QwtWheel",
    "valueChanged",
    "",
    "value",
    "wheelPressed",
    "wheelReleased",
    "wheelMoved",
    "setValue",
    "setTotalAngle",
    "setViewAngle",
    "setMass",
    "orientation",
    "Qt::Orientation",
    "minimum",
    "maximum",
    "singleStep",
    "pageStepCount",
    "stepAlignment",
    "tracking",
    "wrapping",
    "inverted",
    "mass",
    "updateInterval",
    "totalAngle",
    "viewAngle",
    "tickCount",
    "wheelWidth",
    "borderWidth",
    "wheelBorderWidth"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN8QwtWheelE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
      18,   82, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   62,    2, 0x06,   19 /* Public */,
       4,    0,   65,    2, 0x06,   21 /* Public */,
       5,    0,   66,    2, 0x06,   22 /* Public */,
       6,    1,   67,    2, 0x06,   23 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       7,    1,   70,    2, 0x0a,   25 /* Public */,
       8,    1,   73,    2, 0x0a,   27 /* Public */,
       9,    1,   76,    2, 0x0a,   29 /* Public */,
      10,    1,   79,    2, 0x0a,   31 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Double,    3,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Double,    3,

 // slots: parameters
    QMetaType::Void, QMetaType::Double,    2,
    QMetaType::Void, QMetaType::Double,    2,
    QMetaType::Void, QMetaType::Double,    2,
    QMetaType::Void, QMetaType::Double,    2,

 // properties: name, type, flags, notifyId, revision
      11, 0x80000000 | 12, 0x0001510b, uint(-1), 0,
       3, QMetaType::Double, 0x00115103, uint(0), 0,
      13, QMetaType::Double, 0x00015103, uint(-1), 0,
      14, QMetaType::Double, 0x00015103, uint(-1), 0,
      15, QMetaType::Double, 0x00015103, uint(-1), 0,
      16, QMetaType::Int, 0x00015103, uint(-1), 0,
      17, QMetaType::Bool, 0x00015103, uint(-1), 0,
      18, QMetaType::Bool, 0x00015103, uint(-1), 0,
      19, QMetaType::Bool, 0x00015103, uint(-1), 0,
      20, QMetaType::Bool, 0x00015103, uint(-1), 0,
      21, QMetaType::Double, 0x00015103, uint(-1), 0,
      22, QMetaType::Int, 0x00015103, uint(-1), 0,
      23, QMetaType::Double, 0x00015103, uint(-1), 0,
      24, QMetaType::Double, 0x00015103, uint(-1), 0,
      25, QMetaType::Int, 0x00015103, uint(-1), 0,
      26, QMetaType::Int, 0x00015103, uint(-1), 0,
      27, QMetaType::Int, 0x00015103, uint(-1), 0,
      28, QMetaType::Int, 0x00015103, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject QwtWheel::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN8QwtWheelE.offsetsAndSizes,
    qt_meta_data_ZN8QwtWheelE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN8QwtWheelE_t,
        // property 'orientation'
        QtPrivate::TypeAndForceComplete<Qt::Orientation, std::true_type>,
        // property 'value'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'minimum'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'maximum'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'singleStep'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'pageStepCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'stepAlignment'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'tracking'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'wrapping'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'inverted'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'mass'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'updateInterval'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'totalAngle'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'viewAngle'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'tickCount'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'wheelWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'borderWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'wheelBorderWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QwtWheel, std::true_type>,
        // method 'valueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'wheelPressed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'wheelReleased'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'wheelMoved'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'setValue'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'setTotalAngle'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'setViewAngle'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'setMass'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>
    >,
    nullptr
} };

void QwtWheel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QwtWheel *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->valueChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 1: _t->wheelPressed(); break;
        case 2: _t->wheelReleased(); break;
        case 3: _t->wheelMoved((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 4: _t->setValue((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 5: _t->setTotalAngle((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 6: _t->setViewAngle((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 7: _t->setMass((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QwtWheel::*)(double );
            if (_q_method_type _q_method = &QwtWheel::valueChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QwtWheel::*)();
            if (_q_method_type _q_method = &QwtWheel::wheelPressed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QwtWheel::*)();
            if (_q_method_type _q_method = &QwtWheel::wheelReleased; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QwtWheel::*)(double );
            if (_q_method_type _q_method = &QwtWheel::wheelMoved; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< Qt::Orientation*>(_v) = _t->orientation(); break;
        case 1: *reinterpret_cast< double*>(_v) = _t->value(); break;
        case 2: *reinterpret_cast< double*>(_v) = _t->minimum(); break;
        case 3: *reinterpret_cast< double*>(_v) = _t->maximum(); break;
        case 4: *reinterpret_cast< double*>(_v) = _t->singleStep(); break;
        case 5: *reinterpret_cast< int*>(_v) = _t->pageStepCount(); break;
        case 6: *reinterpret_cast< bool*>(_v) = _t->stepAlignment(); break;
        case 7: *reinterpret_cast< bool*>(_v) = _t->isTracking(); break;
        case 8: *reinterpret_cast< bool*>(_v) = _t->wrapping(); break;
        case 9: *reinterpret_cast< bool*>(_v) = _t->isInverted(); break;
        case 10: *reinterpret_cast< double*>(_v) = _t->mass(); break;
        case 11: *reinterpret_cast< int*>(_v) = _t->updateInterval(); break;
        case 12: *reinterpret_cast< double*>(_v) = _t->totalAngle(); break;
        case 13: *reinterpret_cast< double*>(_v) = _t->viewAngle(); break;
        case 14: *reinterpret_cast< int*>(_v) = _t->tickCount(); break;
        case 15: *reinterpret_cast< int*>(_v) = _t->wheelWidth(); break;
        case 16: *reinterpret_cast< int*>(_v) = _t->borderWidth(); break;
        case 17: *reinterpret_cast< int*>(_v) = _t->wheelBorderWidth(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setOrientation(*reinterpret_cast< Qt::Orientation*>(_v)); break;
        case 1: _t->setValue(*reinterpret_cast< double*>(_v)); break;
        case 2: _t->setMinimum(*reinterpret_cast< double*>(_v)); break;
        case 3: _t->setMaximum(*reinterpret_cast< double*>(_v)); break;
        case 4: _t->setSingleStep(*reinterpret_cast< double*>(_v)); break;
        case 5: _t->setPageStepCount(*reinterpret_cast< int*>(_v)); break;
        case 6: _t->setStepAlignment(*reinterpret_cast< bool*>(_v)); break;
        case 7: _t->setTracking(*reinterpret_cast< bool*>(_v)); break;
        case 8: _t->setWrapping(*reinterpret_cast< bool*>(_v)); break;
        case 9: _t->setInverted(*reinterpret_cast< bool*>(_v)); break;
        case 10: _t->setMass(*reinterpret_cast< double*>(_v)); break;
        case 11: _t->setUpdateInterval(*reinterpret_cast< int*>(_v)); break;
        case 12: _t->setTotalAngle(*reinterpret_cast< double*>(_v)); break;
        case 13: _t->setViewAngle(*reinterpret_cast< double*>(_v)); break;
        case 14: _t->setTickCount(*reinterpret_cast< int*>(_v)); break;
        case 15: _t->setWheelWidth(*reinterpret_cast< int*>(_v)); break;
        case 16: _t->setBorderWidth(*reinterpret_cast< int*>(_v)); break;
        case 17: _t->setWheelBorderWidth(*reinterpret_cast< int*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QwtWheel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QwtWheel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN8QwtWheelE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QwtWheel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 8;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    }
    return _id;
}

// SIGNAL 0
void QwtWheel::valueChanged(double _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QwtWheel::wheelPressed()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void QwtWheel::wheelReleased()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void QwtWheel::wheelMoved(double _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
