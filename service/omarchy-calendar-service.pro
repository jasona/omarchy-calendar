QT += core dbus network networkauth sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle

TARGET = omarchy-calendar-service
SOURCES += \
    calendarservice.cpp \
    database.cpp \
    recurrence.cpp \
    googleauth.cpp \
    googlemutations.cpp \
    googlesync.cpp \
    main.cpp

HEADERS += \
    calendarservice.h \
    database.h \
    recurrence.h \
    googleauth.h \
    googlemutations.h \
    googlesync.h \
    secretstore.h

SOURCES += secretstore.cpp
CONFIG += link_pkgconfig
PKGCONFIG += libsecret-1
