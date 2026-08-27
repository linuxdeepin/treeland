// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "debughelpers.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMetaEnum>

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

// Maps a key name to a Qt::Key enum value using Qt's meta-object system.
// Accepts the enum name with or without the "Key_" prefix, case-insensitively
// (e.g. "Escape", "escape", "Key_Escape" all resolve to Qt::Key_Escape).
// Raw integer values pass through unchanged. Sets *ok=false for unknown names.
int keyCode(const QString &name, bool *ok)
{
    *ok = true;
    // Raw integer passthrough (Qt::Key value).
    bool parsed = false;
    const int code = name.toInt(&parsed);
    if (parsed)
        return code;

    // Use QMetaEnum to convert key names to Qt::Key values. The user types
    // "Escape" or "escape"; we match it against "Key_Escape" in the enum.
    static const QMetaEnum meta = QMetaEnum::fromType<Qt::Key>();
    const QString lower = name.toLower();
    const QString lowerWithPrefix = QLatin1String("key_") + lower;

    for (int i = 0; i < meta.keyCount(); ++i) {
        const QString key = QString::fromUtf8(meta.key(i)).toLower();
        if (key == lower || key == lowerWithPrefix)
            return meta.value(i);
    }

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

    // ---- scene [id] (QtQuick scene tree dump) ----
    if (command == QLatin1String("scene")) {
        r.ok = true;
        r.command = DebugCommand::Scene;
        if (!args.isEmpty())
            r.target = args.first();
        return r;
    }

    // ---- listen (HTTP/WebSocket server) ----
    if (command == QLatin1String("listen")) {
        r.ok = true;
        r.command = DebugCommand::Listen;
        for (int i = 0; i < args.size(); ++i) {
            if (args[i] == QLatin1String("--port") && i + 1 < args.size()) {
                r.port = args[++i].toInt();
            } else if (args[i] == QLatin1String("--host") && i + 1 < args.size()) {
                r.host = args[++i];
            } else {
                r.ok = false;
                r.error = QStringLiteral("listen: usage: listen [--port <port>] [--host <addr>]");
                return r;
            }
        }
        if (r.port <= 0 || r.port > 65535) {
            r.ok = false;
            r.error = QStringLiteral("listen: port must be in range 1-65535");
            return r;
        }
        return r;
    }

    r.error = QStringLiteral("unknown command '%1' (try --help)").arg(command);
    return r;
}
