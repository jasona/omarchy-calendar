QT += core network sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = google-delete-undo-test
SOURCES += google-delete-undo-test.cpp ../service/database.cpp ../service/googlemutations.cpp ../service/recurrence.cpp
HEADERS += ../service/database.h ../service/googlemutations.h ../service/recurrence.h
INCLUDEPATH += ../service
