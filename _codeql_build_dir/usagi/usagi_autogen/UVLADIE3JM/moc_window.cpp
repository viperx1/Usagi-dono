/****************************************************************************
** Meta object code from reading C++ file 'window.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../usagi/src/window.h"
#include <QtGui/qtextcursor.h>
#include <QScreen>
#include <QtNetwork/QSslError>
#include <QtNetwork/QSslPreSharedKeyAuthenticator>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'window.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.4.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
namespace {
struct qt_meta_stringdata_Window_t {
    uint offsetsAndSizes[94];
    char stringdata0[7];
    char stringdata1[10];
    char stringdata2[1];
    char stringdata3[17];
    char stringdata4[13];
    char stringdata5[13];
    char stringdata6[13];
    char stringdata7[23];
    char stringdata8[22];
    char stringdata9[23];
    char stringdata10[19];
    char stringdata11[20];
    char stringdata12[21];
    char stringdata13[19];
    char stringdata14[22];
    char stringdata15[25];
    char stringdata16[15];
    char stringdata17[5];
    char stringdata18[13];
    char stringdata19[17];
    char stringdata20[24];
    char stringdata21[17];
    char stringdata22[19];
    char stringdata23[18];
    char stringdata24[19];
    char stringdata25[25];
    char stringdata26[4];
    char stringdata27[8];
    char stringdata28[23];
    char stringdata29[6];
    char stringdata30[22];
    char stringdata31[4];
    char stringdata32[30];
    char stringdata33[30];
    char stringdata34[24];
    char stringdata35[4];
    char stringdata36[4];
    char stringdata37[21];
    char stringdata38[17];
    char stringdata39[5];
    char stringdata40[10];
    char stringdata41[23];
    char stringdata42[20];
    char stringdata43[22];
    char stringdata44[25];
    char stringdata45[26];
    char stringdata46[28];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_Window_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_Window_t qt_meta_stringdata_Window = {
    {
        QT_MOC_LITERAL(0, 6),  // "Window"
        QT_MOC_LITERAL(7, 9),  // "hashFiles"
        QT_MOC_LITERAL(17, 0),  // ""
        QT_MOC_LITERAL(18, 16),  // "notifyStopHasher"
        QT_MOC_LITERAL(35, 12),  // "Button1Click"
        QT_MOC_LITERAL(48, 12),  // "Button2Click"
        QT_MOC_LITERAL(61, 12),  // "Button3Click"
        QT_MOC_LITERAL(74, 22),  // "ButtonHasherStartClick"
        QT_MOC_LITERAL(97, 21),  // "ButtonHasherStopClick"
        QT_MOC_LITERAL(119, 22),  // "ButtonHasherClearClick"
        QT_MOC_LITERAL(142, 18),  // "getNotifyPartsDone"
        QT_MOC_LITERAL(161, 19),  // "getNotifyFileHashed"
        QT_MOC_LITERAL(181, 20),  // "ed2k::ed2kfilestruct"
        QT_MOC_LITERAL(202, 18),  // "getNotifyLogAppend"
        QT_MOC_LITERAL(221, 21),  // "getNotifyLoginChagned"
        QT_MOC_LITERAL(243, 24),  // "getNotifyPasswordChagned"
        QT_MOC_LITERAL(268, 14),  // "hasherFinished"
        QT_MOC_LITERAL(283, 4),  // "shot"
        QT_MOC_LITERAL(288, 12),  // "saveSettings"
        QT_MOC_LITERAL(301, 16),  // "apitesterProcess"
        QT_MOC_LITERAL(318, 23),  // "markwatchedStateChanged"
        QT_MOC_LITERAL(342, 16),  // "ButtonLoginClick"
        QT_MOC_LITERAL(359, 18),  // "getNotifyMylistAdd"
        QT_MOC_LITERAL(378, 17),  // "getNotifyLoggedIn"
        QT_MOC_LITERAL(396, 18),  // "getNotifyLoggedOut"
        QT_MOC_LITERAL(415, 24),  // "getNotifyMessageReceived"
        QT_MOC_LITERAL(440, 3),  // "nid"
        QT_MOC_LITERAL(444, 7),  // "message"
        QT_MOC_LITERAL(452, 22),  // "getNotifyCheckStarting"
        QT_MOC_LITERAL(475, 5),  // "count"
        QT_MOC_LITERAL(481, 21),  // "getNotifyExportQueued"
        QT_MOC_LITERAL(503, 3),  // "tag"
        QT_MOC_LITERAL(507, 29),  // "getNotifyExportAlreadyInQueue"
        QT_MOC_LITERAL(537, 29),  // "getNotifyExportNoSuchTemplate"
        QT_MOC_LITERAL(567, 23),  // "getNotifyEpisodeUpdated"
        QT_MOC_LITERAL(591, 3),  // "eid"
        QT_MOC_LITERAL(595, 3),  // "aid"
        QT_MOC_LITERAL(599, 20),  // "onMylistItemExpanded"
        QT_MOC_LITERAL(620, 16),  // "QTreeWidgetItem*"
        QT_MOC_LITERAL(637, 4),  // "item"
        QT_MOC_LITERAL(642, 9),  // "safeClose"
        QT_MOC_LITERAL(652, 22),  // "loadMylistFromDatabase"
        QT_MOC_LITERAL(675, 19),  // "updateEpisodeInTree"
        QT_MOC_LITERAL(695, 21),  // "startupInitialization"
        QT_MOC_LITERAL(717, 24),  // "isMylistFirstRunComplete"
        QT_MOC_LITERAL(742, 25),  // "setMylistFirstRunComplete"
        QT_MOC_LITERAL(768, 27)   // "requestMylistExportManually"
    },
    "Window",
    "hashFiles",
    "",
    "notifyStopHasher",
    "Button1Click",
    "Button2Click",
    "Button3Click",
    "ButtonHasherStartClick",
    "ButtonHasherStopClick",
    "ButtonHasherClearClick",
    "getNotifyPartsDone",
    "getNotifyFileHashed",
    "ed2k::ed2kfilestruct",
    "getNotifyLogAppend",
    "getNotifyLoginChagned",
    "getNotifyPasswordChagned",
    "hasherFinished",
    "shot",
    "saveSettings",
    "apitesterProcess",
    "markwatchedStateChanged",
    "ButtonLoginClick",
    "getNotifyMylistAdd",
    "getNotifyLoggedIn",
    "getNotifyLoggedOut",
    "getNotifyMessageReceived",
    "nid",
    "message",
    "getNotifyCheckStarting",
    "count",
    "getNotifyExportQueued",
    "tag",
    "getNotifyExportAlreadyInQueue",
    "getNotifyExportNoSuchTemplate",
    "getNotifyEpisodeUpdated",
    "eid",
    "aid",
    "onMylistItemExpanded",
    "QTreeWidgetItem*",
    "item",
    "safeClose",
    "loadMylistFromDatabase",
    "updateEpisodeInTree",
    "startupInitialization",
    "isMylistFirstRunComplete",
    "setMylistFirstRunComplete",
    "requestMylistExportManually"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_Window[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      36,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,  230,    2, 0x06,    1 /* Public */,
       3,    0,  233,    2, 0x06,    3 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       4,    0,  234,    2, 0x0a,    4 /* Public */,
       5,    0,  235,    2, 0x0a,    5 /* Public */,
       6,    0,  236,    2, 0x0a,    6 /* Public */,
       7,    0,  237,    2, 0x0a,    7 /* Public */,
       8,    0,  238,    2, 0x0a,    8 /* Public */,
       9,    0,  239,    2, 0x0a,    9 /* Public */,
      10,    2,  240,    2, 0x0a,   10 /* Public */,
      11,    1,  245,    2, 0x0a,   13 /* Public */,
      13,    1,  248,    2, 0x0a,   15 /* Public */,
      14,    1,  251,    2, 0x0a,   17 /* Public */,
      15,    1,  254,    2, 0x0a,   19 /* Public */,
      16,    0,  257,    2, 0x0a,   21 /* Public */,
      17,    0,  258,    2, 0x0a,   22 /* Public */,
      18,    0,  259,    2, 0x0a,   23 /* Public */,
      19,    0,  260,    2, 0x0a,   24 /* Public */,
      20,    1,  261,    2, 0x0a,   25 /* Public */,
      21,    0,  264,    2, 0x0a,   27 /* Public */,
      22,    2,  265,    2, 0x0a,   28 /* Public */,
      23,    2,  270,    2, 0x0a,   31 /* Public */,
      24,    2,  275,    2, 0x0a,   34 /* Public */,
      25,    2,  280,    2, 0x0a,   37 /* Public */,
      28,    1,  285,    2, 0x0a,   40 /* Public */,
      30,    1,  288,    2, 0x0a,   42 /* Public */,
      32,    1,  291,    2, 0x0a,   44 /* Public */,
      33,    1,  294,    2, 0x0a,   46 /* Public */,
      34,    2,  297,    2, 0x0a,   48 /* Public */,
      37,    1,  302,    2, 0x0a,   51 /* Public */,
      40,    0,  305,    2, 0x0a,   53 /* Public */,
      41,    0,  306,    2, 0x0a,   54 /* Public */,
      42,    2,  307,    2, 0x0a,   55 /* Public */,
      43,    0,  312,    2, 0x0a,   58 /* Public */,
      44,    0,  313,    2, 0x0a,   59 /* Public */,
      45,    0,  314,    2, 0x0a,   60 /* Public */,
      46,    0,  315,    2, 0x0a,   61 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::QStringList,    2,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void, 0x80000000 | 12,    2,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void, QMetaType::QString,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    2,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,    2,    2,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,    2,    2,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,    2,    2,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,   26,   27,
    QMetaType::Void, QMetaType::Int,   29,
    QMetaType::Void, QMetaType::QString,   31,
    QMetaType::Void, QMetaType::QString,   31,
    QMetaType::Void, QMetaType::QString,   31,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   35,   36,
    QMetaType::Void, 0x80000000 | 38,   39,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   35,   36,
    QMetaType::Void,
    QMetaType::Bool,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject Window::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_Window.offsetsAndSizes,
    qt_meta_data_Window,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_Window_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<Window, std::true_type>,
        // method 'hashFiles'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QStringList, std::false_type>,
        // method 'notifyStopHasher'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'Button1Click'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'Button2Click'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'Button3Click'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'ButtonHasherStartClick'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'ButtonHasherStopClick'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'ButtonHasherClearClick'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'getNotifyPartsDone'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getNotifyFileHashed'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<ed2k::ed2kfilestruct, std::false_type>,
        // method 'getNotifyLogAppend'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'getNotifyLoginChagned'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'getNotifyPasswordChagned'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'hasherFinished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'shot'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'saveSettings'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'apitesterProcess'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'markwatchedStateChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'ButtonLoginClick'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'getNotifyMylistAdd'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getNotifyLoggedIn'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getNotifyLoggedOut'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getNotifyMessageReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'getNotifyCheckStarting'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'getNotifyExportQueued'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'getNotifyExportAlreadyInQueue'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'getNotifyExportNoSuchTemplate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'getNotifyEpisodeUpdated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onMylistItemExpanded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QTreeWidgetItem *, std::false_type>,
        // method 'safeClose'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'loadMylistFromDatabase'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateEpisodeInTree'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'startupInitialization'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'isMylistFirstRunComplete'
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'setMylistFirstRunComplete'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'requestMylistExportManually'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void Window::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Window *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->hashFiles((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 1: _t->notifyStopHasher(); break;
        case 2: _t->Button1Click(); break;
        case 3: _t->Button2Click(); break;
        case 4: _t->Button3Click(); break;
        case 5: _t->ButtonHasherStartClick(); break;
        case 6: _t->ButtonHasherStopClick(); break;
        case 7: _t->ButtonHasherClearClick(); break;
        case 8: _t->getNotifyPartsDone((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 9: _t->getNotifyFileHashed((*reinterpret_cast< std::add_pointer_t<ed2k::ed2kfilestruct>>(_a[1]))); break;
        case 10: _t->getNotifyLogAppend((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 11: _t->getNotifyLoginChagned((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 12: _t->getNotifyPasswordChagned((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: _t->hasherFinished(); break;
        case 14: _t->shot(); break;
        case 15: _t->saveSettings(); break;
        case 16: _t->apitesterProcess(); break;
        case 17: _t->markwatchedStateChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 18: _t->ButtonLoginClick(); break;
        case 19: _t->getNotifyMylistAdd((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 20: _t->getNotifyLoggedIn((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 21: _t->getNotifyLoggedOut((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 22: _t->getNotifyMessageReceived((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 23: _t->getNotifyCheckStarting((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 24: _t->getNotifyExportQueued((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 25: _t->getNotifyExportAlreadyInQueue((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 26: _t->getNotifyExportNoSuchTemplate((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 27: _t->getNotifyEpisodeUpdated((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 28: _t->onMylistItemExpanded((*reinterpret_cast< std::add_pointer_t<QTreeWidgetItem*>>(_a[1]))); break;
        case 29: _t->safeClose(); break;
        case 30: _t->loadMylistFromDatabase(); break;
        case 31: _t->updateEpisodeInTree((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 32: _t->startupInitialization(); break;
        case 33: { bool _r = _t->isMylistFirstRunComplete();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 34: _t->setMylistFirstRunComplete(); break;
        case 35: _t->requestMylistExportManually(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (Window::*)(QStringList );
            if (_t _q_method = &Window::hashFiles; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (Window::*)();
            if (_t _q_method = &Window::notifyStopHasher; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject *Window::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Window::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_Window.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int Window::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 36)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 36;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 36)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 36;
    }
    return _id;
}

// SIGNAL 0
void Window::hashFiles(QStringList _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void Window::notifyStopHasher()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
