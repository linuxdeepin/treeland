// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddeshelsurfacewindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);

    QCommandLineParser parser;
    QCommandLineOption autoPlaceOption(
        QStringList{ QStringLiteral("auto_place"), QStringLiteral("auto") },
        ("Set the vertical alignment of the surface within the cursor width"));
    parser.setApplicationDescription("default is TestSetPosition, add --auto is TestSetAutoPlace");
    parser.addHelpOption();
    parser.addOption(autoPlaceOption);
    parser.process(app);

    DDEShelSurfaceWindow::TestMode mode = DDEShelSurfaceWindow::TestMode::TestSetPosition;
    if (parser.isSet(autoPlaceOption))
        mode = DDEShelSurfaceWindow::TestMode::TestSetAutoPlace;

    DDEShelSurfaceWindow window(mode);
    window.setWindowTitle("test for super overlay surface");
    window.resize(600, 200);
    window.show();

    return app.exec();
}
