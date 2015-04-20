HEADERS += src/window.h \
    src/main.h \
    src/hasherthread.h \
    src/anidbapi.h \
    src/hash/md4.h \
    src/hash/ed2k.h
SOURCES += src/window.cpp \
    src/main.cpp \
    src/hasherthread.cpp \
    src/anidbapi.cpp \
    src/hash/md4.cpp \
    src/hash/ed2k.cpp \
    src/anidbapi_settings.cpp
QT += network sql
QT += core gui widgets
