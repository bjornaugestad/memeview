TEMPLATE = app
TARGET = memeview
QT += widgets gui
CONFIG += c++17
CONFIG += debug
QMAKE_CXXFLAGS_DEBUG += -O0 -g -fno-omit-frame-pointer
QMAKE_CFLAGS_DEBUG   += -O0 -g -fno-omit-frame-pointer

SOURCES += main.cpp

