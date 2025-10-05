QT += testlib core
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

TARGET = test_hash

SOURCES += test_hash.cpp \
    ../usagi/src/hash/md4.cpp \
    ../usagi/src/hash/ed2k.cpp

HEADERS += ../usagi/src/hash/md4.h \
    ../usagi/src/hash/ed2k.h

INCLUDEPATH += ../usagi/src

