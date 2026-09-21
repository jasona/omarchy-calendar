QT += core sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = timezone-contract-test
SOURCES += timezone-contract-test.cpp ../service/database.cpp ../app/timezonehelper.cpp
HEADERS += ../service/database.h ../app/timezonehelper.h
INCLUDEPATH += ../service ../app
