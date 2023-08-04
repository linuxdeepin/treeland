#pragma once

#include <QGuiApplication>

class FakeSession : public QGuiApplication {
    Q_OBJECT
public:
    explicit FakeSession(int argc, char* argv[]);
};
