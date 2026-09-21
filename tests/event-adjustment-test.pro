QT += core sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = event-adjustment-test
SOURCES += event-adjustment-test.cpp ../service/database.cpp
HEADERS += ../service/database.h
INCLUDEPATH += ../service
