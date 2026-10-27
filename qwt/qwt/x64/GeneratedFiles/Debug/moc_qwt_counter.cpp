/****************************************************************************
** Meta object code from reading C++ file 'qwt_counter.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/qwt_counter.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qwt_counter.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10QwtCounterE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN10QwtCounterE = QtMocHelpers::stringData(
    "QwtCounter",
    "buttonReleased",
    "",
    "value",
    "valueChanged",
    "setValue",
    "btnReleased",
    "btnClicked",
    "textChanged",
    "minimum",
    "maximum",
    "singleStep",
    "numButtons",
    "stepButton1",
    "stepButton2",
    "stepButton3",
    "readOnly",
    "wrapping"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN10QwtCounterE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       6,   14, // methods
      10,   62, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   50,    2, 0x06,   11 /* Public */,
       4,    1,   53,    2, 0x06,   13 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       5,    1,   56,    2, 0x0a,   15 /* Public */,
       6,    0,   59,    2, 0x08,   17 /* Private */,
       7,    0,   60,    2, 0x08,   18 /* Private */,
       8,    0,   61,    2, 0x08,   19 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::Double,    3,
    QMetaType::Void, QMetaType::Double,    3,

 // slots: parameters
    QMetaType::Void, QMetaType::Double,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // properties: name, type, flags, notifyId, revision
       3, QMetaType::Double, 0x00115103, uint(1), 0,
       9, QMetaType::Double, 0x00015103, uint(-1), 0,
      10, QMetaType::Double, 0x00015103, uint(-1), 0,
      11, QMetaType::Double, 0x00015103, uint(-1), 0,
      12, QMetaType::Int, 0x00015103, uint(-1), 0,
      13, QMetaType::Int, 0x00015103, uint(-1), 0,
      14, QMetaType::Int, 0x00015103, uint(-1), 0,
      15, QMetaType::Int, 0x00015103, uint(-1), 0,
      16, QMetaType::Bool, 0x00015103, uint(-1), 0,
      17, QMetaType::Bool, 0x00015103, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject QwtCounter::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN10QwtCounterE.offsetsAndSizes,
    qt_meta_data_ZN10QwtCounterE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN10QwtCounterE_t,
        // property 'value'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'minimum'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'maximum'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'singleStep'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'numButtons'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'stepButton1'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'stepButton2'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'stepButton3'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'readOnly'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'wrapping'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QwtCounter, std::true_type>,
        // method 'buttonReleased'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'valueChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'setValue'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<double, std::false_type>,
        // method 'btnReleased'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'btnClicked'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'textChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void QwtCounter::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QwtCounter *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->buttonReleased((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 1: _t->valueChanged((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 2: _t->setValue((*reinterpret_cast< std::add_pointer_t<double>>(_a[1]))); break;
        case 3: _t->btnReleased(); break;
        case 4: _t->btnClicked(); break;
        case 5: _t->textChanged(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (QwtCounter::*)(double );
            if (_q_method_type _q_method = &QwtCounter::buttonReleased; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (QwtCounter::*)(double );
            if (_q_method_type _q_method = &QwtCounter::valueChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< double*>(_v) = _t->value(); break;
        case 1: *reinterpret_cast< double*>(_v) = _t->minimum(); break;
        case 2: *reinterpret_cast< double*>(_v) = _t->maximum(); break;
        case 3: *reinterpret_cast< double*>(_v) = _t->singleStep(); break;
        case 4: *reinterpret_cast< int*>(_v) = _t->numButtons(); break;
        case 5: *reinterpret_cast< int*>(_v) = _t->stepButton1(); break;
        case 6: *reinterpret_cast< int*>(_v) = _t->stepButton2(); break;
        case 7: *reinterpret_cast< int*>(_v) = _t->stepButton3(); break;
        case 8: *reinterpret_cast< bool*>(_v) = _t->isReadOnly(); break;
        case 9: *reinterpret_cast< bool*>(_v) = _t->wrapping(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setValue(*reinterpret_cast< double*>(_v)); break;
        case 1: _t->setMinimum(*reinterpret_cast< double*>(_v)); break;
        case 2: _t->setMaximum(*reinterpret_cast< double*>(_v)); break;
        case 3: _t->setSingleStep(*reinterpret_cast< double*>(_v)); break;
        case 4: _t->setNumButtons(*reinterpret_cast< int*>(_v)); break;
        case 5: _t->setStepButton1(*reinterpret_cast< int*>(_v)); break;
        case 6: _t->setStepButton2(*reinterpret_cast< int*>(_v)); break;
        case 7: _t->setStepButton3(*reinterpret_cast< int*>(_v)); break;
        case 8: _t->setReadOnly(*reinterpret_cast< bool*>(_v)); break;
        case 9: _t->setWrapping(*reinterpret_cast< bool*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QwtCounter::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QwtCounter::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN10QwtCounterE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QwtCounter::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void QwtCounter::buttonReleased(double _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void QwtCounter::valueChanged(double _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
