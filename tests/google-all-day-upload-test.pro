QT += core network sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = google-all-day-upload-test
SOURCES += google-all-day-upload-test.cpp ../service/database.cpp ../service/googlemutations.cpp
HEADERS += ../service/database.h ../service/googlemutations.h
INCLUDEPATH += ../service
