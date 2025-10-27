/****************************************************************************
** Meta object code from reading C++ file 'anidbapi.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../usagi/src/anidbapi.h"
#include <QtNetwork/QSslPreSharedKeyAuthenticator>
#include <QtNetwork/QSslError>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'anidbapi.h' doesn't include <QObject>."
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
struct qt_meta_stringdata_AniDBApi_t {
    uint offsetsAndSizes[46];
    char stringdata0[9];
    char stringdata1[16];
    char stringdata2[1];
    char stringdata3[15];
    char stringdata4[16];
    char stringdata5[22];
    char stringdata6[4];
    char stringdata7[8];
    char stringdata8[20];
    char stringdata9[6];
    char stringdata10[19];
    char stringdata11[4];
    char stringdata12[27];
    char stringdata13[27];
    char stringdata14[21];
    char stringdata15[4];
    char stringdata16[4];
    char stringdata17[11];
    char stringdata18[5];
    char stringdata19[22];
    char stringdata20[24];
    char stringdata21[15];
    char stringdata22[6];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_AniDBApi_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_AniDBApi_t qt_meta_stringdata_AniDBApi = {
    {
        QT_MOC_LITERAL(0, 8),  // "AniDBApi"
        QT_MOC_LITERAL(9, 15),  // "notifyMylistAdd"
        QT_MOC_LITERAL(25, 0),  // ""
        QT_MOC_LITERAL(26, 14),  // "notifyLoggedIn"
        QT_MOC_LITERAL(41, 15),  // "notifyLoggedOut"
        QT_MOC_LITERAL(57, 21),  // "notifyMessageReceived"
        QT_MOC_LITERAL(79, 3),  // "nid"
        QT_MOC_LITERAL(83, 7),  // "message"
        QT_MOC_LITERAL(91, 19),  // "notifyCheckStarting"
        QT_MOC_LITERAL(111, 5),  // "count"
        QT_MOC_LITERAL(117, 18),  // "notifyExportQueued"
        QT_MOC_LITERAL(136, 3),  // "tag"
        QT_MOC_LITERAL(140, 26),  // "notifyExportAlreadyInQueue"
        QT_MOC_LITERAL(167, 26),  // "notifyExportNoSuchTemplate"
        QT_MOC_LITERAL(194, 20),  // "notifyEpisodeUpdated"
        QT_MOC_LITERAL(215, 3),  // "eid"
        QT_MOC_LITERAL(219, 3),  // "aid"
        QT_MOC_LITERAL(223, 10),  // "SendPacket"
        QT_MOC_LITERAL(234, 4),  // "Recv"
        QT_MOC_LITERAL(239, 21),  // "checkForNotifications"
        QT_MOC_LITERAL(261, 23),  // "onAnimeTitlesDownloaded"
        QT_MOC_LITERAL(285, 14),  // "QNetworkReply*"
        QT_MOC_LITERAL(300, 5)   // "reply"
    },
    "AniDBApi",
    "notifyMylistAdd",
    "",
    "notifyLoggedIn",
    "notifyLoggedOut",
    "notifyMessageReceived",
    "nid",
    "message",
    "notifyCheckStarting",
    "count",
    "notifyExportQueued",
    "tag",
    "notifyExportAlreadyInQueue",
    "notifyExportNoSuchTemplate",
    "notifyEpisodeUpdated",
    "eid",
    "aid",
    "SendPacket",
    "Recv",
    "checkForNotifications",
    "onAnimeTitlesDownloaded",
    "QNetworkReply*",
    "reply"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_AniDBApi[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       9,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,   92,    2, 0x06,    1 /* Public */,
       3,    2,   97,    2, 0x06,    4 /* Public */,
       4,    2,  102,    2, 0x06,    7 /* Public */,
       5,    2,  107,    2, 0x06,   10 /* Public */,
       8,    1,  112,    2, 0x06,   13 /* Public */,
      10,    1,  115,    2, 0x06,   15 /* Public */,
      12,    1,  118,    2, 0x06,   17 /* Public */,
      13,    1,  121,    2, 0x06,   19 /* Public */,
      14,    2,  124,    2, 0x06,   21 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      17,    0,  129,    2, 0x0a,   24 /* Public */,
      18,    0,  130,    2, 0x0a,   25 /* Public */,
      19,    0,  131,    2, 0x0a,   26 /* Public */,
      20,    1,  132,    2, 0x08,   27 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString, QMetaType::Int,    2,    2,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,    2,    2,
    QMetaType::Void, QMetaType::QString, QMetaType::Int,    2,    2,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,    6,    7,
    QMetaType::Void, QMetaType::Int,    9,
    QMetaType::Void, QMetaType::QString,   11,
    QMetaType::Void, QMetaType::QString,   11,
    QMetaType::Void, QMetaType::QString,   11,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   15,   16,

 // slots: parameters
    QMetaType::Int,
    QMetaType::Int,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 21,   22,

       0        // eod
};

Q_CONSTINIT const QMetaObject AniDBApi::staticMetaObject = { {
    QMetaObject::SuperData::link<ed2k::staticMetaObject>(),
    qt_meta_stringdata_AniDBApi.offsetsAndSizes,
    qt_meta_data_AniDBApi,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_AniDBApi_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<AniDBApi, std::true_type>,
        // method 'notifyMylistAdd'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'notifyLoggedIn'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'notifyLoggedOut'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'notifyMessageReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'notifyCheckStarting'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'notifyExportQueued'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'notifyExportAlreadyInQueue'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'notifyExportNoSuchTemplate'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QString, std::false_type>,
        // method 'notifyEpisodeUpdated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'SendPacket'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'Recv'
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'checkForNotifications'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onAnimeTitlesDownloaded'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QNetworkReply *, std::false_type>
    >,
    nullptr
} };

void AniDBApi::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AniDBApi *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->notifyMylistAdd((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 1: _t->notifyLoggedIn((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 2: _t->notifyLoggedOut((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 3: _t->notifyMessageReceived((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->notifyCheckStarting((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->notifyExportQueued((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->notifyExportAlreadyInQueue((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 7: _t->notifyExportNoSuchTemplate((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: _t->notifyEpisodeUpdated((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 9: { int _r = _t->SendPacket();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 10: { int _r = _t->Recv();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 11: _t->checkForNotifications(); break;
        case 12: _t->onAnimeTitlesDownloaded((*reinterpret_cast< std::add_pointer_t<QNetworkReply*>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 12:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QNetworkReply* >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (AniDBApi::*)(QString , int );
            if (_t _q_method = &AniDBApi::notifyMylistAdd; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (AniDBApi::*)(QString , int );
            if (_t _q_method = &AniDBApi::notifyLoggedIn; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (AniDBApi::*)(QString , int );
            if (_t _q_method = &AniDBApi::notifyLoggedOut; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (AniDBApi::*)(int , QString );
            if (_t _q_method = &AniDBApi::notifyMessageReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (AniDBApi::*)(int );
            if (_t _q_method = &AniDBApi::notifyCheckStarting; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (AniDBApi::*)(QString );
            if (_t _q_method = &AniDBApi::notifyExportQueued; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (AniDBApi::*)(QString );
            if (_t _q_method = &AniDBApi::notifyExportAlreadyInQueue; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
        {
            using _t = void (AniDBApi::*)(QString );
            if (_t _q_method = &AniDBApi::notifyExportNoSuchTemplate; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 7;
                return;
            }
        }
        {
            using _t = void (AniDBApi::*)(int , int );
            if (_t _q_method = &AniDBApi::notifyEpisodeUpdated; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 8;
                return;
            }
        }
    }
}

const QMetaObject *AniDBApi::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AniDBApi::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_AniDBApi.stringdata0))
        return static_cast<void*>(this);
    return ed2k::qt_metacast(_clname);
}

int AniDBApi::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = ed2k::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    }
    return _id;
}

// SIGNAL 0
void AniDBApi::notifyMylistAdd(QString _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void AniDBApi::notifyLoggedIn(QString _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void AniDBApi::notifyLoggedOut(QString _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void AniDBApi::notifyMessageReceived(int _t1, QString _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void AniDBApi::notifyCheckStarting(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void AniDBApi::notifyExportQueued(QString _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void AniDBApi::notifyExportAlreadyInQueue(QString _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}

// SIGNAL 7
void AniDBApi::notifyExportNoSuchTemplate(QString _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 7, _a);
}

// SIGNAL 8
void AniDBApi::notifyEpisodeUpdated(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 8, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
