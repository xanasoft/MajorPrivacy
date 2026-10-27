/****************************************************************************
** Meta object code from reading C++ file 'qwt_abstract_slider.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/qwt_abstract_slider.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qwt_abstract_slider.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN17QwtAbstractSliderE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN17QwtAbstractSliderE = QtMocHelpers::stringData(
    "QwtAbstractSlider",
    "valueChanged",
    "",
    "value",
    "sliderPressed",
    "sliderReleased",
    "sliderMoved",
    "setValue",
    "totalSteps",
    "singleSteps",
    "pageSteps",
    "stepAlignment",
    "readOnly",
    "tracking",
    "wrapping",
    "invertedControls"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN17QwtAbstractSliderE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       9,   55, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   44,    2, 0x06,   10 /* Public */,
       4,    0,   47,    2, 0x06,   12 /* Public */,
       5,    0,   48,    2, 0x06,   13 /* Public */,
       6,    1,   49,    2, 0x06,   14 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       7,    1,   52,    2, 0x0a,   16 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Double,    3,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Double,    3,

 // slots: parameters
    QMetaType::Void, QMetaType::Double,    3,

 // properties: name, type, flags, notifyId, revision
       3, QMetaType::Double, 0x00115103, uint(0), 0,
       8, QMetaType::UInt, 0x00015103, uint(-1), 0,
       9, QMetaType::UInt, 0x00015103, uint(-1), 0,
      10, QMetaType::UInt, 0x00015103, uint(-1), 0,
      11, QMetaType::Bool, 0x00015103, uint(-1), 0,
      12, QMetaType::Bool, 0x00015103, uint(-1), 0,
      13, QMetaType::Bool, 0x00015103, uint(-1), 0,
      14, QMetaType::Bool, 0x00015103, uint(-1), 0,
      15, QMetaType::Bool, 0x00015103, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject QwtAbstractSlider::staticMetaObject = { {
    QMetaObject::SuperData::link<QwtAbstractScale::staticMetaObject>(),
    qt_meta_stringdata_ZN17QwtAbstractSliderE.offsetsAndSizes,
    qt_meta_data_ZN17QwtAbstractSliderE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN17QwtAbstractSliderE_t,
        // property 'value'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'totalSteps'
        QtPrivate::TypeAndForceComplete<uint, std::true_type>,
        // property 'singleSteps'
        QtPrivate::TypeAndForceComplete<uint, std::true_type>,
        // property 'pageSteps'
        QtPrivate::TypeAndForceComplete<uint, std::true_type>,
        // property 'stepAlignment'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'readOnly'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'tracking'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'wrapping'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'invertedControls'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QwtAbstractSlider, std::true_type>,
        // method 'valueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'sliderPressed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'sliderReleased'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'sliderMoved'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'setValue'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>
    >,
    nullptr
} };

void QwtAbstractSlider::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QwtAbstractSlider *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->valueChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 1: _t->sliderPressed(); break;
        case 2: _t->sliderReleased(); break;
        case 3: _t->sliderMoved((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 4: _t->setValue((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QwtAbstractSlider::*)(double );
            if (_q_method_type _q_method = &QwtAbstractSlider::valueChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QwtAbstractSlider::*)();
            if (_q_method_type _q_method = &QwtAbstractSlider::sliderPressed; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (QwtAbstractSlider::*)();
            if (_q_method_type _q_method = &QwtAbstractSlider::sliderReleased; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (QwtAbstractSlider::*)(double );
            if (_q_method_type _q_method = &QwtAbstractSlider::sliderMoved; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< double*>(_v) = _t->value(); break;
        case 1: *reinterpret_cast< uint*>(_v) = _t->totalSteps(); break;
        case 2: *reinterpret_cast< uint*>(_v) = _t->singleSteps(); break;
        case 3: *reinterpret_cast< uint*>(_v) = _t->pageSteps(); break;
        case 4: *reinterpret_cast< bool*>(_v) = _t->stepAlignment(); break;
        case 5: *reinterpret_cast< bool*>(_v) = _t->isReadOnly(); break;
        case 6: *reinterpret_cast< bool*>(_v) = _t->isTracking(); break;
        case 7: *reinterpret_cast< bool*>(_v) = _t->wrapping(); break;
        case 8: *reinterpret_cast< bool*>(_v) = _t->invertedControls(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setValue(*reinterpret_cast< double*>(_v)); break;
        case 1: _t->setTotalSteps(*reinterpret_cast< uint*>(_v)); break;
        case 2: _t->setSingleSteps(*reinterpret_cast< uint*>(_v)); break;
        case 3: _t->setPageSteps(*reinterpret_cast< uint*>(_v)); break;
        case 4: _t->setStepAlignment(*reinterpret_cast< bool*>(_v)); break;
        case 5: _t->setReadOnly(*reinterpret_cast< bool*>(_v)); break;
        case 6: _t->setTracking(*reinterpret_cast< bool*>(_v)); break;
        case 7: _t->setWrapping(*reinterpret_cast< bool*>(_v)); break;
        case 8: _t->setInvertedControls(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QwtAbstractSlider::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QwtAbstractSlider::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN17QwtAbstractSliderE.stringdata0))
        return static_cast<void*>(this);
    return QwtAbstractScale::qt_metacast(_clname);
}

int QwtAbstractSlider::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QwtAbstractScale::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void QwtAbstractSlider::valueChanged(double _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QwtAbstractSlider::sliderPressed()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void QwtAbstractSlider::sliderReleased()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void QwtAbstractSlider::sliderMoved(double _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
