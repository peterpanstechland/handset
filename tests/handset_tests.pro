QT += core gui widgets testlib
CONFIG += c++17 testcase
TARGET = handset_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += handset_tests.h ../src/backend.h ../src/theme.h ../src/translator.h
SOURCES += handset_tests.cpp ../src/backend.cpp ../src/theme.cpp ../src/translator.cpp
