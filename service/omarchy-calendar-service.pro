QT += core dbus network networkauth sql
QT -= gui
CONFIG += console c++20
CONFIG -= app_bundle

TARGET = omarchy-calendar-service
SOURCES += \
    calendarservice.cpp \
    database.cpp \
    googleauth.cpp \
    googlesync.cpp \
    main.cpp

HEADERS += \
    calendarservice.h \
    database.h \
    googleauth.h \
    googlesync.h \
    secretstore.h

SOURCES += secretstore.cpp
CONFIG += link_pkgconfig
PKGCONFIG += libsecret-1
