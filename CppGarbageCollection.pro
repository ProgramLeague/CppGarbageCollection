TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    gc_ptr.cpp \
    main.cpp

include(deployment.pri)
qtcAddDeployment()

HEADERS += \
    gc_ptr.h \
    MemoryChunk.h \
    ObjectManager.h \
    StaticMemory.h

DISTFILES += \
    readme.md

QMAKE_CXXFLAGS += -std=c++11

