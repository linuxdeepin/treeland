// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddeshelsurfacewindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>

#include <cstdio>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);

    QCommandLineParser parser;
    QCommandLineOption positionOption(
        QStringList{ QStringLiteral("pos") },
        QStringLiteral("set_position_hint 的坐标 (x,y)，默认 100,100"),
        QStringLiteral("x,y"));
    QCommandLineOption cursorOffsetOption(
        QStringList{ QStringLiteral("offset") },
        QStringLiteral("set_cursor_placement_hint 的光标偏移 (x,y)，默认 0,30"),
        QStringLiteral("x,y"));
    parser.setApplicationDescription(
        QStringLiteral("treeland_dde_shell_unstable_v2 示例。\n"
                       "--pos 使用 set_position_hint（固定坐标）模式，\n"
                       "--offset 使用 set_cursor_placement_hint（相对光标放置）模式，\n"
                       "两者互斥，只能指定其一；都不指定时默认固定坐标模式。"));
    parser.addHelpOption();
    parser.addOption(positionOption);
    parser.addOption(cursorOffsetOption);
    parser.process(app);

    if (parser.isSet(positionOption) && parser.isSet(cursorOffsetOption)) {
        fputs("error: --pos and --offset are mutually exclusive\n", stderr);
        return 1;
    }

    DDEShelSurfaceWindow::TestMode mode = DDEShelSurfaceWindow::TestMode::TestSetPosition;
    if (parser.isSet(cursorOffsetOption))
        mode = DDEShelSurfaceWindow::TestMode::TestSetCursorPlacement;

    DDEShelSurfaceWindow window(mode);

    if (parser.isSet(positionOption)) {
        const QStringList parts = parser.value(positionOption).split(QLatin1Char(','));
        if (parts.size() == 2)
            window.setPositionHint(QPoint(parts[0].toInt(), parts[1].toInt()));
    }
    if (parser.isSet(cursorOffsetOption)) {
        const QStringList parts = parser.value(cursorOffsetOption).split(QLatin1Char(','));
        if (parts.size() == 2)
            window.setCursorOffset(QPoint(parts[0].toInt(), parts[1].toInt()));
    }

    window.show();

    return app.exec();
}
