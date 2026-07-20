/****************************************************************************
** Meta object code from reading C++ file 'QChartAxis.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../QChartAxis.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'QChartAxis.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10QChartAxisE_t {};
} // unnamed namespace

template <> constexpr inline auto QChartAxis::qt_create_metaobjectdata<qt_meta_tag_ZN10QChartAxisE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "QChartAxis",
        "rangeChanged",
        "",
        "min",
        "max",
        "visibleChanged",
        "tickCountChanged",
        "visible",
        "gridVisible",
        "title",
        "color",
        "QColor",
        "gridColor",
        "tickCount",
        "subTickCount"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'rangeChanged'
        QtMocHelpers::SignalData<void(qreal, qreal)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QReal, 3 }, { QMetaType::QReal, 4 },
        }}),
        // Signal 'visibleChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'tickCountChanged'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'min'
        QtMocHelpers::PropertyData<qreal>(3, QMetaType::QReal, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 0),
        // property 'max'
        QtMocHelpers::PropertyData<qreal>(4, QMetaType::QReal, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 0),
        // property 'visible'
        QtMocHelpers::PropertyData<bool>(7, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 1),
        // property 'gridVisible'
        QtMocHelpers::PropertyData<bool>(8, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet),
        // property 'title'
        QtMocHelpers::PropertyData<QString>(9, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet),
        // property 'color'
        QtMocHelpers::PropertyData<QColor>(10, 0x80000000 | 11, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag | QMC::StdCppSet),
        // property 'gridColor'
        QtMocHelpers::PropertyData<QColor>(12, 0x80000000 | 11, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag | QMC::StdCppSet),
        // property 'tickCount'
        QtMocHelpers::PropertyData<int>(13, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet),
        // property 'subTickCount'
        QtMocHelpers::PropertyData<int>(14, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<QChartAxis, qt_meta_tag_ZN10QChartAxisE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject QChartAxis::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10QChartAxisE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10QChartAxisE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10QChartAxisE_t>.metaTypes,
    nullptr
} };

void QChartAxis::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QChartAxis *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->rangeChanged((*reinterpret_cast<std::add_pointer_t<qreal>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qreal>>(_a[2]))); break;
        case 1: _t->visibleChanged(); break;
        case 2: _t->tickCountChanged(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (QChartAxis::*)(qreal , qreal )>(_a, &QChartAxis::rangeChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (QChartAxis::*)()>(_a, &QChartAxis::visibleChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (QChartAxis::*)()>(_a, &QChartAxis::tickCountChanged, 2))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<qreal*>(_v) = _t->min(); break;
        case 1: *reinterpret_cast<qreal*>(_v) = _t->max(); break;
        case 2: *reinterpret_cast<bool*>(_v) = _t->isVisible(); break;
        case 3: *reinterpret_cast<bool*>(_v) = _t->isGridVisible(); break;
        case 4: *reinterpret_cast<QString*>(_v) = _t->title(); break;
        case 5: *reinterpret_cast<QColor*>(_v) = _t->color(); break;
        case 6: *reinterpret_cast<QColor*>(_v) = _t->gridColor(); break;
        case 7: *reinterpret_cast<int*>(_v) = _t->tickCount(); break;
        case 8: *reinterpret_cast<int*>(_v) = _t->subTickCount(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setMin(*reinterpret_cast<qreal*>(_v)); break;
        case 1: _t->setMax(*reinterpret_cast<qreal*>(_v)); break;
        case 2: _t->setVisible(*reinterpret_cast<bool*>(_v)); break;
        case 3: _t->setGridVisible(*reinterpret_cast<bool*>(_v)); break;
        case 4: _t->setTitle(*reinterpret_cast<QString*>(_v)); break;
        case 5: _t->setColor(*reinterpret_cast<QColor*>(_v)); break;
        case 6: _t->setGridColor(*reinterpret_cast<QColor*>(_v)); break;
        case 7: _t->setTickCount(*reinterpret_cast<int*>(_v)); break;
        case 8: _t->setSubTickCount(*reinterpret_cast<int*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QChartAxis::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QChartAxis::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10QChartAxisE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int QChartAxis::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
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
void QChartAxis::rangeChanged(qreal _t1, qreal _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void QChartAxis::visibleChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void QChartAxis::tickCountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
namespace {
struct qt_meta_tag_ZN10QValueAxisE_t {};
} // unnamed namespace

template <> constexpr inline auto QValueAxis::qt_create_metaobjectdata<qt_meta_tag_ZN10QValueAxisE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "QValueAxis",
        "tickInterval",
        "labelDecimals",
        "labelFormat"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
        // property 'tickInterval'
        QtMocHelpers::PropertyData<qreal>(1, QMetaType::QReal, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet),
        // property 'labelDecimals'
        QtMocHelpers::PropertyData<int>(2, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet),
        // property 'labelFormat'
        QtMocHelpers::PropertyData<QString>(3, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<QValueAxis, qt_meta_tag_ZN10QValueAxisE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject QValueAxis::staticMetaObject = { {
    QMetaObject::SuperData::link<QChartAxis::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10QValueAxisE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10QValueAxisE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10QValueAxisE_t>.metaTypes,
    nullptr
} };

void QValueAxis::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QValueAxis *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<qreal*>(_v) = _t->tickInterval(); break;
        case 1: *reinterpret_cast<int*>(_v) = _t->labelDecimals(); break;
        case 2: *reinterpret_cast<QString*>(_v) = _t->labelFormat(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setTickInterval(*reinterpret_cast<qreal*>(_v)); break;
        case 1: _t->setLabelDecimals(*reinterpret_cast<int*>(_v)); break;
        case 2: _t->setLabelFormat(*reinterpret_cast<QString*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QValueAxis::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QValueAxis::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10QValueAxisE_t>.strings))
        return static_cast<void*>(this);
    return QChartAxis::qt_metacast(_clname);
}

int QValueAxis::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QChartAxis::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    return _id;
}
namespace {
struct qt_meta_tag_ZN16QBarCategoryAxisE_t {};
} // unnamed namespace

template <> constexpr inline auto QBarCategoryAxis::qt_create_metaobjectdata<qt_meta_tag_ZN16QBarCategoryAxisE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "QBarCategoryAxis",
        "categoriesChanged",
        "",
        "categories"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'categoriesChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'categories'
        QtMocHelpers::PropertyData<QStringList>(3, QMetaType::QStringList, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet, 0),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<QBarCategoryAxis, qt_meta_tag_ZN16QBarCategoryAxisE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject QBarCategoryAxis::staticMetaObject = { {
    QMetaObject::SuperData::link<QChartAxis::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16QBarCategoryAxisE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16QBarCategoryAxisE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16QBarCategoryAxisE_t>.metaTypes,
    nullptr
} };

void QBarCategoryAxis::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QBarCategoryAxis *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->categoriesChanged(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (QBarCategoryAxis::*)()>(_a, &QBarCategoryAxis::categoriesChanged, 0))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QStringList*>(_v) = _t->categories(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setCategories(*reinterpret_cast<QStringList*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QBarCategoryAxis::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QBarCategoryAxis::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16QBarCategoryAxisE_t>.strings))
        return static_cast<void*>(this);
    return QChartAxis::qt_metacast(_clname);
}

int QBarCategoryAxis::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QChartAxis::qt_metacall(_c, _id, _a);
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
        _id -= 1;
    }
    return _id;
}

// SIGNAL 0
void QBarCategoryAxis::categoriesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
namespace {
struct qt_meta_tag_ZN8QLogAxisE_t {};
} // unnamed namespace

template <> constexpr inline auto QLogAxis::qt_create_metaobjectdata<qt_meta_tag_ZN8QLogAxisE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "QLogAxis",
        "base"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
        // property 'base'
        QtMocHelpers::PropertyData<qreal>(1, QMetaType::QReal, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<QLogAxis, qt_meta_tag_ZN8QLogAxisE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject QLogAxis::staticMetaObject = { {
    QMetaObject::SuperData::link<QChartAxis::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8QLogAxisE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8QLogAxisE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8QLogAxisE_t>.metaTypes,
    nullptr
} };

void QLogAxis::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QLogAxis *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<qreal*>(_v) = _t->base(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setBase(*reinterpret_cast<qreal*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QLogAxis::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QLogAxis::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8QLogAxisE_t>.strings))
        return static_cast<void*>(this);
    return QChartAxis::qt_metacast(_clname);
}

int QLogAxis::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QChartAxis::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    return _id;
}
namespace {
struct qt_meta_tag_ZN13QDateTimeAxisE_t {};
} // unnamed namespace

template <> constexpr inline auto QDateTimeAxis::qt_create_metaobjectdata<qt_meta_tag_ZN13QDateTimeAxisE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "QDateTimeAxis",
        "format"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
        // property 'format'
        QtMocHelpers::PropertyData<QString>(1, QMetaType::QString, QMC::DefaultPropertyFlags | QMC::Writable | QMC::StdCppSet),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<QDateTimeAxis, qt_meta_tag_ZN13QDateTimeAxisE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject QDateTimeAxis::staticMetaObject = { {
    QMetaObject::SuperData::link<QChartAxis::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13QDateTimeAxisE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13QDateTimeAxisE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13QDateTimeAxisE_t>.metaTypes,
    nullptr
} };

void QDateTimeAxis::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<QDateTimeAxis *>(_o);
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QString*>(_v) = _t->format(); break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: _t->setFormat(*reinterpret_cast<QString*>(_v)); break;
        default: break;
        }
    }
}

const QMetaObject *QDateTimeAxis::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QDateTimeAxis::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13QDateTimeAxisE_t>.strings))
        return static_cast<void*>(this);
    return QChartAxis::qt_metacast(_clname);
}

int QDateTimeAxis::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QChartAxis::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    }
    return _id;
}
QT_WARNING_POP
