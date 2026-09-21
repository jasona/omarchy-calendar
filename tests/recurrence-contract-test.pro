QT += core sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = recurrence-contract-test
SOURCES += recurrence-contract-test.cpp ../service/database.cpp ../service/recurrence.cpp
HEADERS += ../service/database.h ../service/recurrence.h
INCLUDEPATH += ../service
