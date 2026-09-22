QT += core network sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = google-event-move-test
SOURCES += google-event-move-test.cpp ../service/database.cpp ../service/googlemutations.cpp ../service/recurrence.cpp
HEADERS += ../service/database.h ../service/googlemutations.h ../service/recurrence.h
INCLUDEPATH += ../service
