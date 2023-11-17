HEADERS += src/anidbapi.h \
    src/main.h \
    src/window.h \
    src/hasherthread.h \
    src/hash/md4.h \
    src/hash/ed2k.h \
    src/Qt-AES-master/qaesencryption.h
SOURCES += src/anidbapi.cpp \
    src/main.cpp \
    src/window.cpp \
    src/hasherthread.cpp \
    src/hash/md4.cpp \
    src/hash/ed2k.cpp \
    src/anidbapi_settings.cpp \
    src/Qt-AES-master/qaesencryption.cpp
QT += network sql
QT += core gui widgets
