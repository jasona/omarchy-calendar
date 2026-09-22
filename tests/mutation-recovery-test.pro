QT += core sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = mutation-recovery-test
INCLUDEPATH += ../service
SOURCES += mutation-recovery-test.cpp ../service/database.cpp ../service/recurrence.cpp
HEADERS += ../service/database.h ../service/recurrence.h
