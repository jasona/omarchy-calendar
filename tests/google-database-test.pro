QT += core sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle

TARGET = google-database-test
SOURCES += \
    google-database-test.cpp \
    ../service/database.cpp
HEADERS += ../service/database.h
INCLUDEPATH += ../service
