/****************************************************************************
** Meta object code from reading C++ file 'qwt_knob.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/qwt_knob.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'qwt_knob.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN7QwtKnobE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN7QwtKnobE = QtMocHelpers::stringData(
    "QwtKnob",
    "knobStyle",
    "KnobStyle",
    "knobWidth",
    "alignment",
    "Qt::Alignment",
    "totalAngle",
    "numTurns",
    "markerStyle",
    "MarkerStyle",
    "markerSize",
    "borderWidth",
    "Flat",
    "Raised",
    "Sunken",
    "Styled",
    "NoMarker",
    "Tick",
    "Triangle",
    "Dot",
    "Nub",
    "Notch"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN7QwtKnobE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       8,   14, // properties
       2,   54, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // properties: name, type, flags, notifyId, revision
       1, 0x80000000 | 2, 0x0001510b, uint(-1), 0,
       3, QMetaType::Int, 0x00015103, uint(-1), 0,
       4, 0x80000000 | 5, 0x0001510b, uint(-1), 0,
       6, QMetaType::Double, 0x00015103, uint(-1), 0,
       7, QMetaType::Int, 0x00015103, uint(-1), 0,
       8, 0x80000000 | 9, 0x0001510b, uint(-1), 0,
      10, QMetaType::Int, 0x00015103, uint(-1), 0,
      11, QMetaType::Int, 0x00015103, uint(-1), 0,

 // enums: name, alias, flags, count, data
       2,    2, 0x0,    4,   64,
       9,    9, 0x0,    6,   72,

 // enum data: key, value
      12, uint(QwtKnob::Flat),
      13, uint(QwtKnob::Raised),
      14, uint(QwtKnob::Sunken),
      15, uint(QwtKnob::Styled),
      16, uint(QwtKnob::NoMarker),
      17, uint(QwtKnob::Tick),
      18, uint(QwtKnob::Triangle),
      19, uint(QwtKnob::Dot),
      20, uint(QwtKnob::Nub),
      21, uint(QwtKnob::Notch),

       0        // eod
};

Q_CONSTINIT const QMetaObject QwtKnob::staticMetaObject = { {
    QMetaObject::SuperData::link<QwtAbstractSlider::staticMetaObject>(),
    qt_meta_stringdata_ZN7QwtKnobE.offsetsAndSizes,
    qt_meta_data_ZN7QwtKnobE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN7QwtKnobE_t,
        // property 'knobStyle'
        QtPrivate::TypeAndForceComplete<KnobStyle, std::true_type>,
        // property 'knobWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'alignment'
        QtPrivate::TypeAndForceComplete<Qt::Alignment, std::true_type>,
        // property 'totalAngle'
        QtPrivate::TypeAndForceComplete<double, std::true_type>,
        // property 'numTurns'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'markerStyle'
        QtPrivate::TypeAndForceComplete<MarkerStyle, std::true_type>,
        // property 'markerSize'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // property 'borderWidth'
        QtPrivate::TypeAndForceComplete<int, std::true_type>,
        // enum 'KnobStyle'
        QtPrivate::TypeAndForceComplete<QwtKnob::KnobStyle, std::true_type>,
        // enum 'MarkerStyle'
        QtPrivate::TypeAndForceComplete<QwtKnob::MarkerStyle, std::true_type>,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<QwtKnob, std::true_type>
    >,
    nullptr
} };

void QwtKnob::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QwtKnob *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< KnobStyle*>(_v) = _t->knobStyle(); break;
        case 1: *reinterpret_cast< int*>(_v) = _t->knobWidth(); break;
        case 2: *reinterpret_cast< Qt::Alignment*>(_v) = _t->alignment(); break;
        case 3: *reinterpret_cast< double*>(_v) = _t->totalAngle(); break;
        case 4: *reinterpret_cast< int*>(_v) = _t->numTurns(); break;
        case 5: *reinterpret_cast< MarkerStyle*>(_v) = _t->markerStyle(); break;
        case 6: *reinterpret_cast< int*>(_v) = _t->markerSize(); break;
        case 7: *reinterpret_cast< int*>(_v) = _t->borderWidth(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setKnobStyle(*reinterpret_cast< KnobStyle*>(_v)); break;
        case 1: _t->setKnobWidth(*reinterpret_cast< int*>(_v)); break;
        case 2: _t->setAlignment(*reinterpret_cast< Qt::Alignment*>(_v)); break;
        case 3: _t->setTotalAngle(*reinterpret_cast< double*>(_v)); break;
        case 4: _t->setNumTurns(*reinterpret_cast< int*>(_v)); break;
        case 5: _t->setMarkerStyle(*reinterpret_cast< MarkerStyle*>(_v)); break;
        case 6: _t->setMarkerSize(*reinterpret_cast< int*>(_v)); break;
        case 7: _t->setBorderWidth(*reinterpret_cast< int*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QwtKnob::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QwtKnob::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN7QwtKnobE.stringdata0))
        return static_cast<void*>(this);
    return QwtAbstractSlider::qt_metacast(_clname);
}

int QwtKnob::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QwtAbstractSlider::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}
QT_WARNING_POP
