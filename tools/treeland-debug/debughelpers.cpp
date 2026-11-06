// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "debughelpers.h"

#include <linux/input-event-codes.h>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMetaEnum>

QString stateName(int state)
{
    switch (state) {
    case 0: return QStringLiteral("Normal");
    case 1: return QStringLiteral("Maximized");
    case 2: return QStringLiteral("Fullscreen");
    case 3: return QStringLiteral("Tiling");
    default: return QStringLiteral("Unknown(%1)").arg(state);
    }
}

QString formatDuration(quint64 ms)
{
    // Round to the nearest 100ms up front: the sub-minute format shoots a
    // decimal, and without this 59.99s would render as "60.0s" instead of
    // rolling over to "1m00s".
    const quint64 rounded = (ms + 50) / 100 * 100;
    if (rounded < 60000)
        return QStringLiteral("%1s").arg(rounded / 1000.0, 0, 'f', 1);
    if (rounded < 3600000)
        return QStringLiteral("%1m%2s")
            .arg(rounded / 60000)
            .arg((rounded % 60000) / 1000, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1h%2m")
        .arg(rounded / 3600000)
        .arg((rounded % 3600000) / 60000, 2, 10, QLatin1Char('0'));
}

QString outputsLabel(const QStringList &outputs, const QString &pacing, const QString &owner)
{
    if (outputs.isEmpty())
        return owner;

    QStringList labels;
    labels.reserve(outputs.size());
    for (const QString &name : outputs)
        labels.append(name == pacing ? QLatin1Char('+') + name : name);
    return labels.join(QLatin1Char(','));
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

namespace {

// Qt::Key -> Linux evdev keycode, used to translate a key name into the value
// wlr_seat_keyboard_notify_key() expects. The table is fixed rather than
// derived from the live xkb keymap: injection produces a *physical* key press,
// and a reverse lookup over the keymap is ambiguous there (many keycodes share
// a level-0 symbol on a multi-layout keymap, while keys such as Return or the
// arrow keys have no level-0 symbol at all). Which symbol the injected keycode
// then produces is up to the user's layout, exactly as with a real keyboard.
struct KeyMapping
{
    Qt::Key key;
    int evdev;
};

constexpr KeyMapping kKeyMap[] = {
    // Letters, digits and punctuation, i.e. the US-layout physical block.
    { Qt::Key_A, KEY_A },           { Qt::Key_B, KEY_B },
    { Qt::Key_C, KEY_C },           { Qt::Key_D, KEY_D },
    { Qt::Key_E, KEY_E },           { Qt::Key_F, KEY_F },
    { Qt::Key_G, KEY_G },           { Qt::Key_H, KEY_H },
    { Qt::Key_I, KEY_I },           { Qt::Key_J, KEY_J },
    { Qt::Key_K, KEY_K },           { Qt::Key_L, KEY_L },
    { Qt::Key_M, KEY_M },           { Qt::Key_N, KEY_N },
    { Qt::Key_O, KEY_O },           { Qt::Key_P, KEY_P },
    { Qt::Key_Q, KEY_Q },           { Qt::Key_R, KEY_R },
    { Qt::Key_S, KEY_S },           { Qt::Key_T, KEY_T },
    { Qt::Key_U, KEY_U },           { Qt::Key_V, KEY_V },
    { Qt::Key_W, KEY_W },           { Qt::Key_X, KEY_X },
    { Qt::Key_Y, KEY_Y },           { Qt::Key_Z, KEY_Z },
    { Qt::Key_0, KEY_0 },           { Qt::Key_1, KEY_1 },
    { Qt::Key_2, KEY_2 },           { Qt::Key_3, KEY_3 },
    { Qt::Key_4, KEY_4 },           { Qt::Key_5, KEY_5 },
    { Qt::Key_6, KEY_6 },           { Qt::Key_7, KEY_7 },
    { Qt::Key_8, KEY_8 },           { Qt::Key_9, KEY_9 },
    { Qt::Key_Minus, KEY_MINUS },   { Qt::Key_Equal, KEY_EQUAL },
    { Qt::Key_BracketLeft, KEY_LEFTBRACE },
    { Qt::Key_BracketRight, KEY_RIGHTBRACE },
    { Qt::Key_Backslash, KEY_BACKSLASH },
    { Qt::Key_Semicolon, KEY_SEMICOLON },
    { Qt::Key_Apostrophe, KEY_APOSTROPHE },
    { Qt::Key_QuoteLeft, KEY_GRAVE },   { Qt::Key_Comma, KEY_COMMA },
    { Qt::Key_Period, KEY_DOT },        { Qt::Key_Slash, KEY_SLASH },
    // Whitespace and editing.
    { Qt::Key_Escape, KEY_ESC },        { Qt::Key_Tab, KEY_TAB },
    { Qt::Key_Backspace, KEY_BACKSPACE }, { Qt::Key_Return, KEY_ENTER },
    { Qt::Key_Enter, KEY_KPENTER },     { Qt::Key_Space, KEY_SPACE },
    { Qt::Key_Insert, KEY_INSERT },     { Qt::Key_Delete, KEY_DELETE },
    // Cursor movement.
    { Qt::Key_Home, KEY_HOME },         { Qt::Key_End, KEY_END },
    { Qt::Key_PageUp, KEY_PAGEUP },     { Qt::Key_PageDown, KEY_PAGEDOWN },
    { Qt::Key_Left, KEY_LEFT },         { Qt::Key_Right, KEY_RIGHT },
    { Qt::Key_Up, KEY_UP },             { Qt::Key_Down, KEY_DOWN },
    // Modifiers and locks.
    { Qt::Key_Shift, KEY_LEFTSHIFT },   { Qt::Key_Control, KEY_LEFTCTRL },
    { Qt::Key_Alt, KEY_LEFTALT },       { Qt::Key_Meta, KEY_LEFTMETA },
    { Qt::Key_Super_L, KEY_LEFTMETA },  { Qt::Key_Super_R, KEY_RIGHTMETA },
    { Qt::Key_AltGr, KEY_RIGHTALT },    { Qt::Key_CapsLock, KEY_CAPSLOCK },
    { Qt::Key_NumLock, KEY_NUMLOCK },   { Qt::Key_ScrollLock, KEY_SCROLLLOCK },
    { Qt::Key_Menu, KEY_MENU },
    // Function and system keys.
    { Qt::Key_F1, KEY_F1 },             { Qt::Key_F2, KEY_F2 },
    { Qt::Key_F3, KEY_F3 },             { Qt::Key_F4, KEY_F4 },
    { Qt::Key_F5, KEY_F5 },             { Qt::Key_F6, KEY_F6 },
    { Qt::Key_F7, KEY_F7 },             { Qt::Key_F8, KEY_F8 },
    { Qt::Key_F9, KEY_F9 },             { Qt::Key_F10, KEY_F10 },
    { Qt::Key_F11, KEY_F11 },           { Qt::Key_F12, KEY_F12 },
    { Qt::Key_Print, KEY_SYSRQ },       { Qt::Key_Pause, KEY_PAUSE },
    { Qt::Key_VolumeDown, KEY_VOLUMEDOWN },
    { Qt::Key_VolumeMute, KEY_MUTE },
    { Qt::Key_VolumeUp, KEY_VOLUMEUP },
    { Qt::Key_MediaPlay, KEY_PLAYPAUSE },
    { Qt::Key_MediaStop, KEY_STOPCD },
    { Qt::Key_MediaPrevious, KEY_PREVIOUSSONG },
    { Qt::Key_MediaNext, KEY_NEXTSONG },
    { Qt::Key_MediaRecord, KEY_RECORD },
    { Qt::Key_PowerOff, KEY_POWER },    { Qt::Key_Sleep, KEY_SLEEP },
    { Qt::Key_WakeUp, KEY_WAKEUP },     { Qt::Key_Calculator, KEY_CALC },
    { Qt::Key_HomePage, KEY_HOMEPAGE }, { Qt::Key_LaunchMail, KEY_MAIL },
};

} // namespace

// Maps a key name to a Linux evdev keycode. A name is a Qt::Key enum name with
// or without the "Key_" prefix, case-insensitively ("Escape", "escape",
// "Key_Escape"), resolved through Qt's meta-object system and then translated
// by kKeyMap. Otherwise a decimal number is taken as a raw evdev keycode, the
// same convention as evtest and ydotool. Names win over numbers, so "5" is
// Qt::Key_5 and not evdev 5 (KEY_4). Sets *ok=false when nothing matches.
int keyCode(const QString &name, bool *ok)
{
    *ok = true;
    // Trimmed up front: a stray space would otherwise miss the name lookup and
    // silently fall through to the numeric branch (" 5" -> evdev 5 = KEY_4).
    const QString trimmed = name.trimmed();
    static const QMetaEnum meta = QMetaEnum::fromType<Qt::Key>();
    const QString lower = trimmed.toLower();
    const QString lowerWithPrefix = QLatin1String("key_") + lower;

    for (int i = 0; i < meta.keyCount(); ++i) {
        const QString key = QString::fromUtf8(meta.key(i)).toLower();
        if (key != lower && key != lowerWithPrefix)
            continue;
        const auto qtKey = static_cast<Qt::Key>(meta.value(i));
        for (const auto &mapping : kKeyMap) {
            if (mapping.key == qtKey)
                return mapping.evdev;
        }
        // A known Qt::Key without an evdev keycode.
        *ok = false;
        return 0;
    }

    bool parsed = false;
    const int numeric = trimmed.toInt(&parsed);
    // Keymaps cover keycodes 1..255; anything else cannot be pressed.
    if (parsed && numeric > 0 && numeric <= 255)
        return numeric;

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
    if (command == QLatin1String("idle")) {
        r.ok = true;
        r.command = DebugCommand::Idle;
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
    if (command == QLatin1String("no-auth")) {
        r.ok = true;
        r.command = DebugCommand::NoAuth;
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
