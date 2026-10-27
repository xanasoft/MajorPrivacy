/****************************************************************************
** Meta object code from reading C++ file 'qwt_slider.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/qwt_slider.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qwt_slider.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN9QwtSliderE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN9QwtSliderE = QtMocHelpers::stringData(
    "QwtSlider",
    "orientation",
    "Qt::Orientation",
    "scalePosition",
    "ScalePosition",
    "trough",
    "groove",
    "handleSize",
    "borderWidth",
    "spacing",
    "NoScale",
    "LeadingScale",
    "TrailingScale"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN9QwtSliderE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       7,   14, // properties
       1,   49, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // properties: name, type, flags, notifyId, revision
       1, 0x80000000 | 2, 0x0001510b, uint(-1), 0,
       3, 0x80000000 | 4, 0x0001510b, uint(-1), 0,
       5, QMetaType::Bool, 0x00015103, uint(-1), 0,
       6, QMetaType::Bool, 0x00015103, uint(-1), 0,
       7, QMetaType::QSize, 0x00015103, uint(-1), 0,
       8, QMetaType::Int, 0x00015103, uint(-1), 0,
       9, QMetaType::Int, 0x00015103, uint(-1), 0,

 // enums: name, alias, flags, count, data
       4,    4, 0x0,    3,   54,

 // enum data: key, value
      10, uint(QwtSlider::NoScale),
      11, uint(QwtSlider::LeadingScale),
      12, uint(QwtSlider::TrailingScale),

       0        // eod
};

Q_CONSTINIT const QMetaObject QwtSlider::staticMetaObject = { {
    QMetaObject::SuperData::link<QwtAbstractSlider::staticMetaObject>(),
    qt_meta_stringdata_ZN9QwtSliderE.offsetsAndSizes,
    qt_meta_data_ZN9QwtSliderE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN9QwtSliderE_t,
        // property 'orientation'
        QtPrivate::TypeAndForceComplete<Qt::Orientation, std::true_type>,
        // property 'scalePosition'
        QtPrivate::TypeAndForceComplete<ScalePosition, std::true_type>,
        // property 'trough'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'groove'
        QtPrivate::TypeAndForceComplete<bool, std::true_type>,
        // property 'handleSize'
        QtPrivate::TypeAndForceComplete<QSize, std::true_type>,
        // property 'borderWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'spacing'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // enum 'ScalePosition'
        QtPrivate::TypeAndForceComplete<QwtSlider::ScalePosition, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QwtSlider, std::true_type>
    >,
    nullptr
} };

void QwtSlider::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QwtSlider *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< Qt::Orientation*>(_v) = _t->orientation(); break;
        case 1: *reinterpret_cast< ScalePosition*>(_v) = _t->scalePosition(); break;
        case 2: *reinterpret_cast< bool*>(_v) = _t->hasTrough(); break;
        case 3: *reinterpret_cast< bool*>(_v) = _t->hasGroove(); break;
        case 4: *reinterpret_cast< QSize*>(_v) = _t->handleSize(); break;
        case 5: *reinterpret_cast< int*>(_v) = _t->borderWidth(); break;
        case 6: *reinterpret_cast< int*>(_v) = _t->spacing(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setOrientation(*reinterpret_cast< Qt::Orientation*>(_v)); break;
        case 1: _t->setScalePosition(*reinterpret_cast< ScalePosition*>(_v)); break;
        case 2: _t->setTrough(*reinterpret_cast< bool*>(_v)); break;
        case 3: _t->setGroove(*reinterpret_cast< bool*>(_v)); break;
        case 4: _t->setHandleSize(*reinterpret_cast< QSize*>(_v)); break;
        case 5: _t->setBorderWidth(*reinterpret_cast< int*>(_v)); break;
        case 6: _t->setSpacing(*reinterpret_cast< int*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QwtSlider::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QwtSlider::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN9QwtSliderE.stringdata0))
        return static_cast<void*>(this);
    return QwtAbstractSlider::qt_metacast(_clname);
}

int QwtSlider::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QwtAbstractSlider::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    return _id;
}
QT_WARNING_POP
