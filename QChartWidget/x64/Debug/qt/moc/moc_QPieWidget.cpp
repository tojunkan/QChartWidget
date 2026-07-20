/****************************************************************************
** Meta object code from reading C++ file 'QPieWidget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../QPieWidget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'QPieWidget.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
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
struct qt_meta_tag_ZN10QPieWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto QPieWidget::qt_create_metaobjectdata<qt_meta_tag_ZN10QPieWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "QPieWidget",
        "countChanged",
        "",
        "newCount",
        "sliceAdded",
        "index",
        "sliceRemoved",
        "sliceValueChanged",
        "newValue",
        "sliceLabelChanged",
        "newLabel",
        "sliceClicked",
        "slicePressed",
        "sliceReleased",
        "sliceHovered",
        "hovered",
        "onExplodeAnimationFinished"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'countChanged'
        QtMocHelpers::SignalData<void(int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Signal 'sliceAdded'
        QtMocHelpers::SignalData<void(int)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 },
        }}),
        // Signal 'sliceRemoved'
        QtMocHelpers::SignalData<void(int)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 },
        }}),
        // Signal 'sliceValueChanged'
        QtMocHelpers::SignalData<void(int, qreal)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 }, { QMetaType::QReal, 8 },
        }}),
        // Signal 'sliceLabelChanged'
        QtMocHelpers::SignalData<void(int, const QString &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 }, { QMetaType::QString, 10 },
        }}),
        // Signal 'sliceClicked'
        QtMocHelpers::SignalData<void(int)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 },
        }}),
        // Signal 'slicePressed'
        QtMocHelpers::SignalData<void(int)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 },
        }}),
        // Signal 'sliceReleased'
        QtMocHelpers::SignalData<void(int)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 },
        }}),
        // Signal 'sliceHovered'
        QtMocHelpers::SignalData<void(int, bool)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 }, { QMetaType::Bool, 15 },
        }}),
        // Slot 'onExplodeAnimationFinished'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<QPieWidget, qt_meta_tag_ZN10QPieWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject QPieWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10QPieWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10QPieWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10QPieWidgetE_t>.metaTypes,
    nullptr
} };

void QPieWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QPieWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->countChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->sliceAdded((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->sliceRemoved((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->sliceValueChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qreal>>(_a[2]))); break;
        case 4: _t->sliceLabelChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 5: _t->sliceClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->slicePressed((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->sliceReleased((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 8: _t->sliceHovered((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 9: _t->onExplodeAnimationFinished(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (QPieWidget::*)(int )>(_a, &QPieWidget::countChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (QPieWidget::*)(int )>(_a, &QPieWidget::sliceAdded, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (QPieWidget::*)(int )>(_a, &QPieWidget::sliceRemoved, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (QPieWidget::*)(int , qreal )>(_a, &QPieWidget::sliceValueChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (QPieWidget::*)(int , const QString & )>(_a, &QPieWidget::sliceLabelChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (QPieWidget::*)(int )>(_a, &QPieWidget::sliceClicked, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (QPieWidget::*)(int )>(_a, &QPieWidget::slicePressed, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (QPieWidget::*)(int )>(_a, &QPieWidget::sliceReleased, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (QPieWidget::*)(int , bool )>(_a, &QPieWidget::sliceHovered, 8))
            return;
    }
}

const QMetaObject *QPieWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QPieWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10QPieWidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QPieWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void QPieWidget::countChanged(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void QPieWidget::sliceAdded(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void QPieWidget::sliceRemoved(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void QPieWidget::sliceValueChanged(int _t1, qreal _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}

// SIGNAL 4
void QPieWidget::sliceLabelChanged(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void QPieWidget::sliceClicked(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void QPieWidget::slicePressed(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void QPieWidget::sliceReleased(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void QPieWidget::sliceHovered(int _t1, bool _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1, _t2);
}
QT_WARNING_POP
