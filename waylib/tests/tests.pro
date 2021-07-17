TEMPLATE = app
CONFIG -= app_bundle
CONFIG += thread

CONFIG += testcase no_testcase_installs

INCLUDEPATH += $$PWD/../src
DEPENDPATH += $$PWD/../src
unix:QMAKE_RPATHDIR += $$OUT_PWD/../src
unix:LIBS += -L$$OUT_PWD/../src/ -lgtest

HEADERS += \
    test.h

SOURCES += \
        main.cpp
