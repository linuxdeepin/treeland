// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "debughelpers.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>

QString stateName(int state)
{
    switch (state) {
    case 0: return QStringLiteral("Normal");
    case 1: return QStringLiteral("Maximized");
    case 2: return QStringLiteral("Minimized");
    case 3: return QStringLiteral("Fullscreen");
    case 4: return QStringLiteral("Tiling");
    default: return QStringLiteral("Unknown(%1)").arg(state);
    }
}

int buttonCode(const QString &name, bool *ok)
{
    *ok = true;
    const QString lower = name.toLower();
    if (lower == QLatin1String("left"))
        return 0x110; // BTN_LEFT
    if (lower == QLatin1String("right"))
        return 0x111; // BTN_RIGHT
    if (lower == QLatin1String("middle"))
        return 0x112; // BTN_MIDDLE
    bool parsed = false;
    const int code = name.toInt(&parsed);
    if (parsed)
        return code;
    *ok = false;
    return 0;
}

int keyCode(const QString &name, bool *ok)
{
    *ok = true;
    bool parsed = false;
    const int code = name.toInt(&parsed);
    if (parsed)
        return code;

    static const QHash<QString, int> table = {
        {QStringLiteral("esc"), 1},     {QStringLiteral("1"), 2},
        {QStringLiteral("2"), 3},       {QStringLiteral("3"), 4},
        {QStringLiteral("4"), 5},       {QStringLiteral("5"), 6},
        {QStringLiteral("6"), 7},       {QStringLiteral("7"), 8},
        {QStringLiteral("8"), 9},       {QStringLiteral("9"), 10},
        {QStringLiteral("0"), 11},      {QStringLiteral("minus"), 12},
        {QStringLiteral("equal"), 13},  {QStringLiteral("backspace"), 14},
        {QStringLiteral("tab"), 15},    {QStringLiteral("q"), 16},
        {QStringLiteral("w"), 17},      {QStringLiteral("e"), 18},
        {QStringLiteral("r"), 19},      {QStringLiteral("t"), 20},
        {QStringLiteral("y"), 21},      {QStringLiteral("u"), 22},
        {QStringLiteral("i"), 23},      {QStringLiteral("o"), 24},
        {QStringLiteral("p"), 25},      {QStringLiteral("enter"), 28},
        {QStringLiteral("leftctrl"), 29}, {QStringLiteral("a"), 30},
        {QStringLiteral("s"), 31},      {QStringLiteral("d"), 32},
        {QStringLiteral("f"), 33},      {QStringLiteral("g"), 34},
        {QStringLiteral("h"), 35},      {QStringLiteral("j"), 36},
        {QStringLiteral("k"), 37},      {QStringLiteral("l"), 38},
        {QStringLiteral("space"), 57},  {QStringLiteral("leftshift"), 42},
        {QStringLiteral("leftalt"), 56},{QStringLiteral("z"), 44},
        {QStringLiteral("x"), 45},      {QStringLiteral("c"), 46},
        {QStringLiteral("v"), 47},      {QStringLiteral("b"), 48},
        {QStringLiteral("n"), 49},      {QStringLiteral("m"), 50},
        {QStringLiteral("left"), 105},  {QStringLiteral("right"), 106},
        {QStringLiteral("up"), 103},    {QStringLiteral("down"), 108},
        {QStringLiteral("f1"), 59},     {QStringLiteral("f2"), 60},
        {QStringLiteral("f3"), 61},     {QStringLiteral("f4"), 62},
        {QStringLiteral("f5"), 63},     {QStringLiteral("f6"), 64},
        {QStringLiteral("f7"), 65},     {QStringLiteral("f8"), 66},
        {QStringLiteral("f9"), 67},     {QStringLiteral("f10"), 68},
        {QStringLiteral("f11"), 87},    {QStringLiteral("f12"), 88},
        {QStringLiteral("del"), 111},   {QStringLiteral("insert"), 110},
        {QStringLiteral("home"), 102},  {QStringLiteral("end"), 107},
        {QStringLiteral("pageup"), 104},{QStringLiteral("pagedown"), 109},
    };
    const auto it = table.constFind(name.toLower());
    if (it != table.constEnd())
        return it.value();
    *ok = false;
    return 0;
}

QString saveCapture(const QByteArray &data, const QString &userPath)
{
    if (data.isEmpty())
        return {};
    QString path = userPath;
    if (path.isEmpty())
        path = QStringLiteral("/tmp/treeland-debug-%1.png").arg(QDateTime::currentMSecsSinceEpoch());
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".png");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return {};
    if (f.write(data) != data.size())
        return {};
    f.close();
    return path;
}

QJsonObject pointToJson(const QPointF &point)
{
    return {
        {"x", point.x()},
        {"y", point.y()},
    };
}

QJsonObject rectToJson(const QRectF &rect)
{
    return {
        {"x", rect.x()},
        {"y", rect.y()},
        {"width", rect.width()},
        {"height", rect.height()},
    };
}

ParseResult parseCommand(const QString &command, const QStringList &args)
{
    ParseResult r;

    // ---- no-argument commands ----
    if (command == QLatin1String("tree")) {
        r.ok = true;
        r.command = DebugCommand::Tree;
        return r;
    }
    if (command == QLatin1String("cursor")) {
        r.ok = true;
        r.command = DebugCommand::Cursor;
        return r;
    }
    if (command == QLatin1String("windows")) {
        r.ok = true;
        r.command = DebugCommand::Windows;
        return r;
    }
    if (command == QLatin1String("clients")) {
        r.ok = true;
        r.command = DebugCommand::Clients;
        return r;
    }
    if (command == QLatin1String("help")) {
        r.ok = true;
        r.command = DebugCommand::Help;
        return r;
    }
    if (command == QLatin1String("shell")) {
        r.ok = true;
        r.command = DebugCommand::Shell;
        return r;
    }

    // ---- window control (target only) ----
    if (command == QLatin1String("activate") || command == QLatin1String("close")
        || command == QLatin1String("minimize") || command == QLatin1String("maximize")
        || command == QLatin1String("fullscreen")) {
        if (args.isEmpty()) {
            r.error = QStringLiteral("%1: missing window target (id or appId)").arg(command);
            return r;
        }
        r.ok = true;
        r.target = args.first();
        if (command == QLatin1String("activate"))
            r.command = DebugCommand::Activate;
        else if (command == QLatin1String("close"))
            r.command = DebugCommand::Close;
        else if (command == QLatin1String("minimize"))
            r.command = DebugCommand::Minimize;
        else if (command == QLatin1String("maximize"))
            r.command = DebugCommand::Maximize;
        else
            r.command = DebugCommand::Fullscreen;
        return r;
    }

    // ---- move <id> <x> <y> ----
    if (command == QLatin1String("move")) {
        if (args.isEmpty()) {
            r.error = QStringLiteral("move: missing window target (id or appId)");
            return r;
        }
        if (args.size() < 3) {
            r.error = QStringLiteral("move: usage: move <id> <x> <y>");
            return r;
        }
        r.ok = true;
        r.command = DebugCommand::Move;
        r.target = args[0];
        r.x = args[1].toInt();
        r.y = args[2].toInt();
        return r;
    }

    // ---- resize <id> <w> <h> ----
    if (command == QLatin1String("resize")) {
        if (args.isEmpty()) {
            r.error = QStringLiteral("resize: missing window target (id or appId)");
            return r;
        }
        if (args.size() < 3) {
            r.error = QStringLiteral("resize: usage: resize <id> <w> <h>");
            return r;
        }
        r.ok = true;
        r.command = DebugCommand::Resize;
        r.target = args[0];
        r.width = args[1].toInt();
        r.height = args[2].toInt();
        return r;
    }

    // ---- workspace <id> <workspace-id> ----
    if (command == QLatin1String("workspace")) {
        if (args.isEmpty()) {
            r.error = QStringLiteral("workspace: missing window target (id or appId)");
            return r;
        }
        if (args.size() < 2) {
            r.error = QStringLiteral("workspace: usage: workspace <id> <workspace-id>");
            return r;
        }
        r.ok = true;
        r.command = DebugCommand::Workspace;
        r.target = args[0];
        r.workspaceId = args[1].toInt();
        return r;
    }

    // ---- move-cursor <x> <y> ----
    if (command == QLatin1String("move-cursor")) {
        if (args.size() < 2) {
            r.error = QStringLiteral("move-cursor: usage: move-cursor <x> <y>");
            return r;
        }
        r.ok = true;
        r.command = DebugCommand::MoveCursor;
        r.dx = args[0].toDouble();
        r.dy = args[1].toDouble();
        return r;
    }

    // ---- event motion|button|key ... ----
    if (command == QLatin1String("event")) {
        if (args.isEmpty()) {
            r.error = QStringLiteral("event: usage: event motion|button|key ...");
            return r;
        }
        const QString sub = args[0];
        if (sub == QLatin1String("motion")) {
            if (args.size() < 3) {
                r.error = QStringLiteral("event motion: usage: event motion <x> <y>");
                return r;
            }
            r.ok = true;
            r.command = DebugCommand::EventMotion;
            r.dx = args[1].toDouble();
            r.dy = args[2].toDouble();
            return r;
        }
        if (sub == QLatin1String("button")) {
            if (args.size() < 2) {
                r.error = QStringLiteral(
                    "event button: usage: event button <left|right|middle|code> [press|release|click]");
                return r;
            }
            bool codeOk = false;
            const int code = buttonCode(args[1], &codeOk);
            if (!codeOk) {
                r.error = QStringLiteral("unknown button '%1'").arg(args[1]);
                return r;
            }
            r.ok = true;
            r.command = DebugCommand::EventButton;
            r.code = code;
            r.action = args.size() > 2 ? args[2] : QStringLiteral("click");
            return r;
        }
        if (sub == QLatin1String("key")) {
            if (args.size() < 2) {
                r.error = QStringLiteral("event key: usage: event key <name|code> [press|release|tap]");
                return r;
            }
            bool keyOk = false;
            const int code = keyCode(args[1], &keyOk);
            if (!keyOk) {
                r.error = QStringLiteral("unknown key '%1'").arg(args[1]);
                return r;
            }
            r.ok = true;
            r.command = DebugCommand::EventKey;
            r.code = code;
            r.action = args.size() > 2 ? args[2] : QStringLiteral("tap");
            return r;
        }
        r.error = QStringLiteral("event: unknown subcommand '%1'").arg(sub);
        return r;
    }

    // ---- screenshot output|window ... ----
    if (command == QLatin1String("screenshot")) {
        if (args.isEmpty()) {
            r.error = QStringLiteral("screenshot: usage: screenshot output|window ...");
            return r;
        }
        const QString sub = args[0];
        if (sub == QLatin1String("output")) {
            r.ok = true;
            r.command = DebugCommand::ScreenshotOutput;
            if (args.size() >= 2 && !args[1].isEmpty())
                r.outputName = args[1];
            if (args.size() >= 3)
                r.filePath = args[2];
            return r;
        }
        if (sub == QLatin1String("window")) {
            if (args.size() < 2) {
                r.error = QStringLiteral("screenshot window: usage: screenshot window <id> [file]");
                return r;
            }
            r.ok = true;
            r.command = DebugCommand::ScreenshotWindow;
            r.target = args[1];
            if (args.size() >= 3)
                r.filePath = args[2];
            return r;
        }
        r.error = QStringLiteral("screenshot: unknown target '%1'").arg(sub);
        return r;
    }

    // ---- live commands ----
    if (command == QLatin1String("top")) {
        r.ok = true;
        r.command = DebugCommand::Top;
        r.intervalMs = 1000;
        if (!args.isEmpty())
            r.intervalMs = args.first().toInt();
        if (r.intervalMs <= 0)
            r.intervalMs = 1000;
        return r;
    }
    if (command == QLatin1String("events")) {
        r.ok = true;
        r.command = DebugCommand::Events;
        r.intervalMs = 50;
        if (!args.isEmpty())
            r.intervalMs = args.first().toInt();
        if (r.intervalMs <= 0)
            r.intervalMs = 50;
        return r;
    }
    if (command == QLatin1String("watch")) {
        if (args.isEmpty()) {
            r.error = QStringLiteral("watch: usage: watch <id> [interval-ms]");
            return r;
        }
        r.ok = true;
        r.command = DebugCommand::Watch;
        r.target = args.first();
        r.intervalMs = 250;
        if (args.size() >= 2)
            r.intervalMs = args[1].toInt();
        if (r.intervalMs <= 0)
            r.intervalMs = 250;
        return r;
    }

    r.error = QStringLiteral("unknown command '%1' (try --help)").arg(command);
    return r;
}
