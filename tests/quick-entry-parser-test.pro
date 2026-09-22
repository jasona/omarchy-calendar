QT += core
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = quick-entry-parser-test
SOURCES += quick-entry-parser-test.cpp ../app/quickentryparser.cpp
HEADERS += ../app/quickentryparser.h
INCLUDEPATH += ../app
