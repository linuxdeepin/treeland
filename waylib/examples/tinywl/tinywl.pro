TEMPLATE = app
QT += quick quickcontrols2

SOURCES += \
    main.cpp

RESOURCES += \
    qml.qrc

win32:CONFIG(release, debug|release): LIBS += -L$$OUT_PWD/../../src/server/release -lwaylibserver
else:win32:CONFIG(debug, debug|release): LIBS += -L$$OUT_PWD/../../src/server/debug -lwaylibserver
else:unix: LIBS += -L$$OUT_PWD/../../src/server -lwaylibserver
INCLUDEPATH += \
    $$PWD/../../src \
    $$PWD/../../src/server \
    $$PWD/../../src/server/kernel \
    $$PWD/../../src/server/qtquick

CONFIG(debug, debug|release) {
    unix:QMAKE_RPATHDIR += $$OUT_PWD/../../src/server
}
