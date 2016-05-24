include(../../qtcreatorplugin.pri)

QT += xml core

HEADERS += \
    vsprojectplugin.h \
    vsmanager.h \
    vsprojectfile.h \
    vsprojectnode.h \
    vsproject.h \
    vsbuildconfiguration.h \
    vsprojectconstants.h \
    vsprojectdata.h \
    devenvstep.h \
    vsrunconfiguration.h

SOURCES += \
    vsprojectplugin.cpp \
    vsmanager.cpp \
    vsprojectfile.cpp \
    vsprojectnode.cpp \
    vsproject.cpp \
    vsbuildconfiguration.cpp \
    vsprojectdata.cpp \
    devenvstep.cpp \
    vsrunconfiguration.cpp

RESOURCES += \
    vsprojectmanager.qrc
