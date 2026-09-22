QT += core gui
CONFIG += console c++20
CONFIG -= app_bundle
TARGET = theme-accessibility-test
INCLUDEPATH += ../app
SOURCES += theme-accessibility-test.cpp ../app/themeprovider.cpp
HEADERS += ../app/themeprovider.h
