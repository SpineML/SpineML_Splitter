#-------------------------------------------------
#
# Project created by QtCreator 2012-06-06T10:49:07
#
#-------------------------------------------------

QT       += core
QT       -= gui
QT       += xml

QMAKE_CXXFLAGS += -fopenmp -g

TARGET = splitter
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
    modelobjects.cpp \
    splitter.cpp \
    writer.cpp \
    xmlwriter.cpp \
    infoparser.cpp \
    parser.cpp \
    aliaswriter.cpp \
    graphwriter.cpp

HEADERS += \
    modelobjects.h \
    splitter.h \
    writer.h \
    xmlwriter.h \
    infoparser.h \
    parser.h \
    aliaswriter.h \
    graphwriter.h

LIBS += -fopenmp
