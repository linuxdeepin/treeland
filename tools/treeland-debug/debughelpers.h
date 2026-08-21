// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef TREELAND_DEBUG_HELPERS_H
#define TREELAND_DEBUG_HELPERS_H

#include <QByteArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

// Maps a window state enum to a human-readable name.
QString stateName(int state);

// Maps a friendly button name to a Linux input button code; raw codes pass
// through. Sets *ok=false for unknown names.
int buttonCode(const QString &name, bool *ok);

// Maps a friendly key name to a Linux evdev keycode; raw codes pass through.
// Sets *ok=false for unknown names.
int keyCode(const QString &name, bool *ok);

// Writes captured image bytes to @p userPath (or a generated /tmp path when
// empty), ensuring a .png suffix. Returns the path written, or empty on
// failure.
QString saveCapture(const QByteArray &data, const QString &userPath);

// JSON helpers for geometry primitives.
QJsonObject pointToJson(const QPointF &point);
QJsonObject rectToJson(const QRectF &rect);

// Identifies which treeland-debug command was recognised by parseCommand().
enum class DebugCommand {
    Tree,
    Cursor,
    Windows,
    Clients,
    Help,
    Shell,
    Activate,
    Close,
    Minimize,
    Maximize,
    Fullscreen,
    Move,
    Resize,
    Workspace,
    MoveCursor,
    EventMotion,
    EventButton,
    EventKey,
    ScreenshotOutput,
    ScreenshotWindow,
    Top,
    Events,
    Watch,
    Listen,
    Unknown,
};

// Outcome of parsing a treeland-debug command line.  When @p ok is false,
// @p error carries the user-facing message (matching the original inline
// messages from main.cpp).  When @p ok is true, @p command identifies the
// command and the remaining fields hold the parsed parameters ready for
// execution against a live Session.
struct ParseResult
{
    bool ok = false;
    QString error;
    DebugCommand command = DebugCommand::Unknown;

    // Window target token (for resolveTarget): activate/close/minimize/
    // maximize/fullscreen/move/resize/workspace/screenshot-window/watch.
    QString target;

    // Integer coordinates/size for move/resize/workspace.
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int workspaceId = 0;

    // Floating-point coordinates for move-cursor / event motion.
    double dx = 0.0;
    double dy = 0.0;

    // Event injection: resolved button/key code and action string.
    int code = 0;
    QString action;

    // Screenshot: output name and file path.
    QString outputName;
    QString filePath;

    // Live commands: polling interval in milliseconds.
    int intervalMs = 0;

    // Listen command: HTTP/WebSocket server bind address.
    int port = 8080;
    QString host = QStringLiteral("0.0.0.0");
};

// Parses a command verb and its argument list into a ParseResult.  Performs
// every validation step that does not require a live Session connection
// (missing arguments, unknown button/key names, unknown subcommands).  On
// failure result.ok is false and result.error holds the message; on
// success result.ok is true and the struct fields are populated.
ParseResult parseCommand(const QString &command, const QStringList &args);

#endif // TREELAND_DEBUG_HELPERS_H
