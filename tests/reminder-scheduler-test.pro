QT += core sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = reminder-scheduler-test
INCLUDEPATH += ../service
SOURCES += reminder-scheduler-test.cpp ../service/database.cpp
HEADERS += ../service/database.h
