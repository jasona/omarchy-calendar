QT += core sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = all-day-multiday-test
SOURCES += all-day-multiday-test.cpp ../service/database.cpp
HEADERS += ../service/database.h
INCLUDEPATH += ../service
