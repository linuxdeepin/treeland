TARGET = waylib
TEMPLATE = subdirs
SUBDIRS += src/server

CONFIG(debug, debug|release) {
    SUBDIRS += examples
    examples.depends = src/server
}
