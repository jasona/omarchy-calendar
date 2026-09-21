QT += core network sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = google-mutation-upload-test
SOURCES += google-mutation-upload-test.cpp ../service/database.cpp ../service/googlemutations.cpp
HEADERS += ../service/database.h ../service/googlemutations.h
INCLUDEPATH += ../service
