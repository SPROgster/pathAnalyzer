#-------------------------------------------------
#
# Project created by QtCreator 2014-06-16T11:48:17
#
#-------------------------------------------------

QT       += core gui testlib

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = pathAnalyzer
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp \
    morphology.cpp

HEADERS  += mainwindow.h \
    morphology.h

FORMS    += mainwindow.ui
