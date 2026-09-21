QT += core network sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle

TARGET = google-sync-retry-test
SOURCES += \
    google-sync-retry-test.cpp \
    ../service/database.cpp \
    ../service/googlesync.cpp
HEADERS += \
    ../service/database.h \
    ../service/googlesync.h
INCLUDEPATH += ../service
