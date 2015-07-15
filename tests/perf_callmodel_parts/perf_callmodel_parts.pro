include( ../../common-project-config.pri )
include( ../../common-vars.pri )
include( ../performance_tests.pri )

TARGET = perf_callmodel_parts
DESTDIR = ../perf_bin
QT -= gui
SOURCES += callmodelperftest.cpp
HEADERS += callmodelperftest.h

