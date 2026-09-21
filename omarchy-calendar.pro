QT += core dbus gui qml quick quickcontrols2
CONFIG += c++20

TARGET = omarchy-calendar
SOURCES += \
    app/eventstore.cpp \
    app/main.cpp \
    app/preferences.cpp \
    app/themeprovider.cpp

HEADERS += \
    app/eventstore.h \
    app/preferences.h \
    app/themeprovider.h
RESOURCES += app/resources/qml.qrc
