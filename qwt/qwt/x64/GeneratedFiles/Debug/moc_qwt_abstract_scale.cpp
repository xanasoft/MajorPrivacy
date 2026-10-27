/****************************************************************************
** Meta object code from reading C++ file 'qwt_abstract_scale.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/qwt_abstract_scale.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qwt_abstract_scale.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN16QwtAbstractScaleE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN16QwtAbstractScaleE = QtMocHelpers::stringData(
    "QwtAbstractScale",
    "lowerBound",
    "upperBound",
    "scaleMaxMajor",
    "scaleMaxMinor",
    "scaleStepSize"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN16QwtAbstractScaleE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       5,   14, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // properties: name, type, flags, notifyId, revision
       1, QMetaType::Double, 0x00015103, uint(-1), 0,
       2, QMetaType::Double, 0x00015103, uint(-1), 0,
       3, QMetaType::Int, 0x00015103, uint(-1), 0,
       4, QMetaType::Int, 0x00015103, uint(-1), 0,
       5, QMetaType::Double, 0x00015103, uint(-1), 0,

       0        // eod
};

Q_CONSTINIT const QMetaObject QwtAbstractScale::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_ZN16QwtAbstractScaleE.offsetsAndSizes,
    qt_meta_data_ZN16QwtAbstractScaleE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN16QwtAbstractScaleE_t,
        // property 'lowerBound'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'upperBound'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'scaleMaxMajor'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'scaleMaxMinor'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'scaleStepSize'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QwtAbstractScale, std::true_type>
    >,
    nullptr
} };

void QwtAbstractScale::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QwtAbstractScale *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< double*>(_v) = _t->lowerBound(); break;
        case 1: *reinterpret_cast< double*>(_v) = _t->upperBound(); break;
        case 2: *reinterpret_cast< int*>(_v) = _t->scaleMaxMajor(); break;
        case 3: *reinterpret_cast< int*>(_v) = _t->scaleMaxMinor(); break;
        case 4: *reinterpret_cast< double*>(_v) = _t->scaleStepSize(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setLowerBound(*reinterpret_cast< double*>(_v)); break;
        case 1: _t->setUpperBound(*reinterpret_cast< double*>(_v)); break;
        case 2: _t->setScaleMaxMajor(*reinterpret_cast< int*>(_v)); break;
        case 3: _t->setScaleMaxMinor(*reinterpret_cast< int*>(_v)); break;
        case 4: _t->setScaleStepSize(*reinterpret_cast< double*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QwtAbstractScale::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QwtAbstractScale::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN16QwtAbstractScaleE.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QwtAbstractScale::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}
QT_WARNING_POP
