#include "clipboard.h"

#include <QGuiApplication>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    Clipboard board;

    return app.exec();
}
