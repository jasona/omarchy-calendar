QT += core sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = performance-smoke-test
INCLUDEPATH += ../service
SOURCES += performance-smoke-test.cpp ../service/database.cpp
HEADERS += ../service/database.h
