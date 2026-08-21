// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "debughelpers.h"

#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QTemporaryDir>
#include <QTest>

// Unit tests for the pure-logic helpers shared by the treeland-debug tool
// (stateName, buttonCode, keyCode, saveCapture, pointToJson, rectToJson)
// plus exhaustive coverage of parseCommand() — the command recognition and
// parameter validation layer that every subcommand flows through.  These do
// not require a running compositor or a Wayland connection.
class TreelandDebugTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // --- pure helpers ---
    void testStateNameKnownStates();
    void testStateNameUnknown();
    void testButtonCodeNamed();
    void testButtonCodeRaw();
    void testButtonCodeUnknown();
    void testKeyCodeNamed();
    void testKeyCodeRaw();
    void testKeyCodeUnknown();
    void testKeyCodeCaseInsensitive();
    void testSaveCaptureExplicitPath();
    void testSaveCaptureAddsPngSuffix();
    void testSaveCaptureEmptyData();
    void testSaveCaptureAutoPath();
    void testPointToJson();
    void testRectToJson();

    // --- parseCommand: no-argument commands ---
    void testParseTree();
    void testParseCursor();
    void testParseWindows();
    void testParseClients();
    void testParseHelp();
    void testParseShell();

    // --- parseCommand: window control ---
    void testParseActivateMissingTarget();
    void testParseActivate();
    void testParseCloseMissingTarget();
    void testParseClose();
    void testParseMinimizeMissingTarget();
    void testParseMinimize();
    void testParseMaximizeMissingTarget();
    void testParseMaximize();
    void testParseFullscreenMissingTarget();
    void testParseFullscreen();

    // --- parseCommand: move / resize / workspace ---
    void testParseMoveMissingTarget();
    void testParseMoveTooFewArgs();
    void testParseMove();
    void testParseResizeMissingTarget();
    void testParseResizeTooFewArgs();
    void testParseResize();
    void testParseWorkspaceMissingTarget();
    void testParseWorkspaceTooFewArgs();
    void testParseWorkspace();

    // --- parseCommand: move-cursor ---
    void testParseMoveCursorTooFewArgs();
    void testParseMoveCursor();
    void testParseMoveCursorNegative();

    // --- parseCommand: event ---
    void testParseEventNoSub();
    void testParseEventUnknownSub();
    void testParseEventMotionTooFewArgs();
    void testParseEventMotion();
    void testParseEventButtonTooFewArgs();
    void testParseEventButtonUnknown();
    void testParseEventButtonNamed();
    void testParseEventButtonRaw();
    void testParseEventButtonWithAction();
    void testParseEventButtonDefaultAction();
    void testParseEventKeyTooFewArgs();
    void testParseEventKeyUnknown();
    void testParseEventKeyNamed();
    void testParseEventKeyRaw();
    void testParseEventKeyWithAction();
    void testParseEventKeyDefaultAction();

    // --- parseCommand: screenshot ---
    void testParseScreenshotNoSub();
    void testParseScreenshotUnknownTarget();
    void testParseScreenshotOutputNoArgs();
    void testParseScreenshotOutputWithName();
    void testParseScreenshotOutputWithNameAndFile();
    void testParseScreenshotOutputEmptyName();
    void testParseScreenshotWindowTooFewArgs();
    void testParseScreenshotWindow();
    void testParseScreenshotWindowWithFile();

    // --- parseCommand: live commands (top/events/watch) ---
    void testParseTopDefault();
    void testParseTopCustomInterval();
    void testParseTopInvalidInterval();
    void testParseEventsDefault();
    void testParseEventsCustomInterval();
    void testParseEventsInvalidInterval();
    void testParseWatchMissingId();
    void testParseWatch();
    void testParseWatchWithInterval();
    void testParseWatchInvalidInterval();

    // --- parseCommand: unknown command ---
    void testParseUnknownCommand();

    // --- parseCommand: listen (HTTP/WebSocket server) ---
    void testParseListenDefault();
    void testParseListenCustomPort();
    void testParseListenCustomHost();
    void testParseListenCustomBoth();
    void testParseListenInvalidPortLow();
    void testParseListenInvalidPortHigh();
    void testParseListenUnknownArg();
    void testParseListenPortNoValue();
};

// ---------------------------------------------------------------------------
// Pure-helper tests (unchanged from original coverage)
// ---------------------------------------------------------------------------

void TreelandDebugTest::testStateNameKnownStates()
{
    QCOMPARE(stateName(0), QStringLiteral("Normal"));
    QCOMPARE(stateName(1), QStringLiteral("Maximized"));
    QCOMPARE(stateName(2), QStringLiteral("Minimized"));
    QCOMPARE(stateName(3), QStringLiteral("Fullscreen"));
    QCOMPARE(stateName(4), QStringLiteral("Tiling"));
}

void TreelandDebugTest::testStateNameUnknown()
{
    QCOMPARE(stateName(99), QStringLiteral("Unknown(99)"));
    QCOMPARE(stateName(-1), QStringLiteral("Unknown(-1)"));
}

void TreelandDebugTest::testButtonCodeNamed()
{
    bool ok = false;
    QCOMPARE(buttonCode(QStringLiteral("left"), &ok), 0x110);
    QVERIFY(ok);
    QCOMPARE(buttonCode(QStringLiteral("right"), &ok), 0x111);
    QVERIFY(ok);
    QCOMPARE(buttonCode(QStringLiteral("middle"), &ok), 0x112);
    QVERIFY(ok);
    // Names are case-insensitive.
    QCOMPARE(buttonCode(QStringLiteral("LEFT"), &ok), 0x110);
    QVERIFY(ok);
}

void TreelandDebugTest::testButtonCodeRaw()
{
    bool ok = false;
    // buttonCode passes raw codes through QString::toInt (base 10), so use
    // decimal values; hex prefixes like "0x113" are not recognized here.
    QCOMPARE(buttonCode(QStringLiteral("275"), &ok), 275); // 0x113 == 275
    QVERIFY(ok);
    QCOMPARE(buttonCode(QStringLiteral("277"), &ok), 277);
    QVERIFY(ok);
}

void TreelandDebugTest::testButtonCodeUnknown()
{
    bool ok = true;
    QCOMPARE(buttonCode(QStringLiteral("foobar"), &ok), 0);
    QVERIFY(!ok);
}

void TreelandDebugTest::testKeyCodeNamed()
{
    bool ok = false;
    QCOMPARE(keyCode(QStringLiteral("esc"), &ok), 1);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("enter"), &ok), 28);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("space"), &ok), 57);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("up"), &ok), 103);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("f12"), &ok), 88);
    QVERIFY(ok);
}

void TreelandDebugTest::testKeyCodeRaw()
{
    bool ok = false;
    QCOMPARE(keyCode(QStringLiteral("200"), &ok), 200);
    QVERIFY(ok);
}

void TreelandDebugTest::testKeyCodeUnknown()
{
    bool ok = true;
    QCOMPARE(keyCode(QStringLiteral("nonexistent"), &ok), 0);
    QVERIFY(!ok);
}

void TreelandDebugTest::testKeyCodeCaseInsensitive()
{
    bool ok = false;
    QCOMPARE(keyCode(QStringLiteral("ESC"), &ok), 1);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("Enter"), &ok), 28);
    QVERIFY(ok);
}

void TreelandDebugTest::testSaveCaptureExplicitPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("capture.png");
    const QByteArray data("PNG-fake-bytes");
    const QString saved = saveCapture(data, path);
    QCOMPARE(saved, path);
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), data);
}

void TreelandDebugTest::testSaveCaptureAddsPngSuffix()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString base = dir.filePath("noext");
    const QByteArray data("data");
    const QString saved = saveCapture(data, base);
    QVERIFY(saved.endsWith(QStringLiteral(".png")));
    QVERIFY(QFile::exists(saved));
}

void TreelandDebugTest::testSaveCaptureEmptyData()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("empty.png");
    const QString saved = saveCapture(QByteArray(), path);
    QVERIFY(saved.isEmpty());
    QVERIFY(!QFile::exists(path));
}

void TreelandDebugTest::testSaveCaptureAutoPath()
{
    const QByteArray data("autodata");
    const QString saved = saveCapture(data, QString());
    QVERIFY(!saved.isEmpty());
    QVERIFY(saved.startsWith(QStringLiteral("/tmp/treeland-debug-")));
    QVERIFY(saved.endsWith(QStringLiteral(".png")));
    QVERIFY(QFile::exists(saved));
    QFile::remove(saved);
}

void TreelandDebugTest::testPointToJson()
{
    const auto obj = pointToJson(QPointF(1.5, -2.0));
    QCOMPARE(obj.value("x").toDouble(), 1.5);
    QCOMPARE(obj.value("y").toDouble(), -2.0);
}

void TreelandDebugTest::testRectToJson()
{
    const auto obj = rectToJson(QRectF(10, 20, 300, 400));
    QCOMPARE(obj.value("x").toDouble(), 10.0);
    QCOMPARE(obj.value("y").toDouble(), 20.0);
    QCOMPARE(obj.value("width").toDouble(), 300.0);
    QCOMPARE(obj.value("height").toDouble(), 400.0);
}

// ---------------------------------------------------------------------------
// parseCommand: no-argument commands
// ---------------------------------------------------------------------------

void TreelandDebugTest::testParseTree()
{
    const auto r = parseCommand(QStringLiteral("tree"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Tree);
    QVERIFY(r.error.isEmpty());
}

void TreelandDebugTest::testParseCursor()
{
    const auto r = parseCommand(QStringLiteral("cursor"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Cursor);
}

void TreelandDebugTest::testParseWindows()
{
    const auto r = parseCommand(QStringLiteral("windows"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Windows);
}

void TreelandDebugTest::testParseClients()
{
    const auto r = parseCommand(QStringLiteral("clients"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Clients);
}

void TreelandDebugTest::testParseHelp()
{
    const auto r = parseCommand(QStringLiteral("help"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Help);
}

void TreelandDebugTest::testParseShell()
{
    const auto r = parseCommand(QStringLiteral("shell"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Shell);
}

// ---------------------------------------------------------------------------
// parseCommand: window control (activate/close/minimize/maximize/fullscreen)
// ---------------------------------------------------------------------------

void TreelandDebugTest::testParseActivateMissingTarget()
{
    const auto r = parseCommand(QStringLiteral("activate"), {});
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("activate: missing window target (id or appId)"));
    QCOMPARE(r.command, DebugCommand::Unknown);
}

void TreelandDebugTest::testParseActivate()
{
    const auto r = parseCommand(QStringLiteral("activate"), { QStringLiteral("42") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Activate);
    QCOMPARE(r.target, QStringLiteral("42"));
}

void TreelandDebugTest::testParseCloseMissingTarget()
{
    const auto r = parseCommand(QStringLiteral("close"), {});
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("close: missing window target (id or appId)"));
}

void TreelandDebugTest::testParseClose()
{
    const auto r = parseCommand(QStringLiteral("close"), { QStringLiteral("org.foo.bar") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Close);
    QCOMPARE(r.target, QStringLiteral("org.foo.bar"));
}

void TreelandDebugTest::testParseMinimizeMissingTarget()
{
    const auto r = parseCommand(QStringLiteral("minimize"), {});
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("minimize: missing window target (id or appId)"));
}

void TreelandDebugTest::testParseMinimize()
{
    const auto r = parseCommand(QStringLiteral("minimize"), { QStringLiteral("100") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Minimize);
    QCOMPARE(r.target, QStringLiteral("100"));
}

void TreelandDebugTest::testParseMaximizeMissingTarget()
{
    const auto r = parseCommand(QStringLiteral("maximize"), {});
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("maximize: missing window target (id or appId)"));
}

void TreelandDebugTest::testParseMaximize()
{
    const auto r = parseCommand(QStringLiteral("maximize"), { QStringLiteral("7") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Maximize);
    QCOMPARE(r.target, QStringLiteral("7"));
}

void TreelandDebugTest::testParseFullscreenMissingTarget()
{
    const auto r = parseCommand(QStringLiteral("fullscreen"), {});
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("fullscreen: missing window target (id or appId)"));
}

void TreelandDebugTest::testParseFullscreen()
{
    const auto r = parseCommand(QStringLiteral("fullscreen"), { QStringLiteral("55") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Fullscreen);
    QCOMPARE(r.target, QStringLiteral("55"));
}

// ---------------------------------------------------------------------------
// parseCommand: move / resize / workspace
// ---------------------------------------------------------------------------

void TreelandDebugTest::testParseMoveMissingTarget()
{
    const auto r = parseCommand(QStringLiteral("move"), {});
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("move: missing window target (id or appId)"));
}

void TreelandDebugTest::testParseMoveTooFewArgs()
{
    const auto r = parseCommand(QStringLiteral("move"), { QStringLiteral("42") });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("move: usage: move <id> <x> <y>"));
}

void TreelandDebugTest::testParseMove()
{
    const auto r = parseCommand(QStringLiteral("move"),
                                { QStringLiteral("42"), QStringLiteral("100"), QStringLiteral("-200") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Move);
    QCOMPARE(r.target, QStringLiteral("42"));
    QCOMPARE(r.x, 100);
    QCOMPARE(r.y, -200);
}

void TreelandDebugTest::testParseResizeMissingTarget()
{
    const auto r = parseCommand(QStringLiteral("resize"), {});
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("resize: missing window target (id or appId)"));
}

void TreelandDebugTest::testParseResizeTooFewArgs()
{
    const auto r = parseCommand(QStringLiteral("resize"), { QStringLiteral("42") });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("resize: usage: resize <id> <w> <h>"));
}

void TreelandDebugTest::testParseResize()
{
    const auto r = parseCommand(QStringLiteral("resize"),
                                { QStringLiteral("42"), QStringLiteral("800"), QStringLiteral("600") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Resize);
    QCOMPARE(r.target, QStringLiteral("42"));
    QCOMPARE(r.width, 800);
    QCOMPARE(r.height, 600);
}

void TreelandDebugTest::testParseWorkspaceMissingTarget()
{
    const auto r = parseCommand(QStringLiteral("workspace"), {});
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("workspace: missing window target (id or appId)"));
}

void TreelandDebugTest::testParseWorkspaceTooFewArgs()
{
    const auto r = parseCommand(QStringLiteral("workspace"), { QStringLiteral("42") });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("workspace: usage: workspace <id> <workspace-id>"));
}

void TreelandDebugTest::testParseWorkspace()
{
    const auto r = parseCommand(QStringLiteral("workspace"),
                                { QStringLiteral("42"), QStringLiteral("3") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Workspace);
    QCOMPARE(r.target, QStringLiteral("42"));
    QCOMPARE(r.workspaceId, 3);
}

// ---------------------------------------------------------------------------
// parseCommand: move-cursor
// ---------------------------------------------------------------------------

void TreelandDebugTest::testParseMoveCursorTooFewArgs()
{
    const auto r = parseCommand(QStringLiteral("move-cursor"), { QStringLiteral("10") });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("move-cursor: usage: move-cursor <x> <y>"));
}

void TreelandDebugTest::testParseMoveCursor()
{
    const auto r = parseCommand(QStringLiteral("move-cursor"),
                                { QStringLiteral("12.5"), QStringLiteral("-7.0") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::MoveCursor);
    QCOMPARE(r.dx, 12.5);
    QCOMPARE(r.dy, -7.0);
}

void TreelandDebugTest::testParseMoveCursorNegative()
{
    const auto r = parseCommand(QStringLiteral("move-cursor"),
                                { QStringLiteral("-100"), QStringLiteral("-200") });
    QVERIFY(r.ok);
    QCOMPARE(r.dx, -100.0);
    QCOMPARE(r.dy, -200.0);
}

// ---------------------------------------------------------------------------
// parseCommand: event motion|button|key
// ---------------------------------------------------------------------------

void TreelandDebugTest::testParseEventNoSub()
{
    const auto r = parseCommand(QStringLiteral("event"), {});
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("event: usage: event motion|button|key ..."));
}

void TreelandDebugTest::testParseEventUnknownSub()
{
    const auto r = parseCommand(QStringLiteral("event"), { QStringLiteral("foobar") });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("event: unknown subcommand 'foobar'"));
}

void TreelandDebugTest::testParseEventMotionTooFewArgs()
{
    const auto r = parseCommand(QStringLiteral("event"), { QStringLiteral("motion"), QStringLiteral("5") });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("event motion: usage: event motion <x> <y>"));
}

void TreelandDebugTest::testParseEventMotion()
{
    const auto r = parseCommand(QStringLiteral("event"),
                                { QStringLiteral("motion"), QStringLiteral("3.5"), QStringLiteral("9.0") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::EventMotion);
    QCOMPARE(r.dx, 3.5);
    QCOMPARE(r.dy, 9.0);
}

void TreelandDebugTest::testParseEventButtonTooFewArgs()
{
    const auto r = parseCommand(QStringLiteral("event"), { QStringLiteral("button") });
    QVERIFY(!r.ok);
    QCOMPARE(r.error,
             QStringLiteral("event button: usage: event button <left|right|middle|code> [press|release|click]"));
}

void TreelandDebugTest::testParseEventButtonUnknown()
{
    const auto r = parseCommand(QStringLiteral("event"),
                                { QStringLiteral("button"), QStringLiteral("foobar") });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("unknown button 'foobar'"));
}

void TreelandDebugTest::testParseEventButtonNamed()
{
    const auto r = parseCommand(QStringLiteral("event"),
                                { QStringLiteral("button"), QStringLiteral("left") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::EventButton);
    QCOMPARE(r.code, 0x110);
    QCOMPARE(r.action, QStringLiteral("click")); // default action
}

void TreelandDebugTest::testParseEventButtonRaw()
{
    const auto r = parseCommand(QStringLiteral("event"),
                                { QStringLiteral("button"), QStringLiteral("275") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::EventButton);
    QCOMPARE(r.code, 275);
    QCOMPARE(r.action, QStringLiteral("click"));
}

void TreelandDebugTest::testParseEventButtonWithAction()
{
    const auto r = parseCommand(QStringLiteral("event"),
                                { QStringLiteral("button"), QStringLiteral("right"), QStringLiteral("press") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::EventButton);
    QCOMPARE(r.code, 0x111);
    QCOMPARE(r.action, QStringLiteral("press"));
}

void TreelandDebugTest::testParseEventButtonDefaultAction()
{
    const auto r = parseCommand(QStringLiteral("event"),
                                { QStringLiteral("button"), QStringLiteral("middle") });
    QVERIFY(r.ok);
    QCOMPARE(r.action, QStringLiteral("click"));
}

void TreelandDebugTest::testParseEventKeyTooFewArgs()
{
    const auto r = parseCommand(QStringLiteral("event"), { QStringLiteral("key") });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("event key: usage: event key <name|code> [press|release|tap]"));
}

void TreelandDebugTest::testParseEventKeyUnknown()
{
    const auto r = parseCommand(QStringLiteral("event"),
                                { QStringLiteral("key"), QStringLiteral("nonexistent") });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("unknown key 'nonexistent'"));
}

void TreelandDebugTest::testParseEventKeyNamed()
{
    const auto r = parseCommand(QStringLiteral("event"),
                                { QStringLiteral("key"), QStringLiteral("enter") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::EventKey);
    QCOMPARE(r.code, 28);
    QCOMPARE(r.action, QStringLiteral("tap")); // default action
}

void TreelandDebugTest::testParseEventKeyRaw()
{
    const auto r = parseCommand(QStringLiteral("event"),
                                { QStringLiteral("key"), QStringLiteral("200") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::EventKey);
    QCOMPARE(r.code, 200);
    QCOMPARE(r.action, QStringLiteral("tap"));
}

void TreelandDebugTest::testParseEventKeyWithAction()
{
    const auto r = parseCommand(QStringLiteral("event"),
                                { QStringLiteral("key"), QStringLiteral("space"), QStringLiteral("release") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::EventKey);
    QCOMPARE(r.code, 57);
    QCOMPARE(r.action, QStringLiteral("release"));
}

void TreelandDebugTest::testParseEventKeyDefaultAction()
{
    const auto r = parseCommand(QStringLiteral("event"),
                                { QStringLiteral("key"), QStringLiteral("esc") });
    QVERIFY(r.ok);
    QCOMPARE(r.action, QStringLiteral("tap"));
}

// ---------------------------------------------------------------------------
// parseCommand: screenshot output|window
// ---------------------------------------------------------------------------

void TreelandDebugTest::testParseScreenshotNoSub()
{
    const auto r = parseCommand(QStringLiteral("screenshot"), {});
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("screenshot: usage: screenshot output|window ..."));
}

void TreelandDebugTest::testParseScreenshotUnknownTarget()
{
    const auto r = parseCommand(QStringLiteral("screenshot"), { QStringLiteral("screen") });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("screenshot: unknown target 'screen'"));
}

void TreelandDebugTest::testParseScreenshotOutputNoArgs()
{
    const auto r = parseCommand(QStringLiteral("screenshot"), { QStringLiteral("output") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::ScreenshotOutput);
    QVERIFY(r.outputName.isEmpty());
    QVERIFY(r.filePath.isEmpty());
}

void TreelandDebugTest::testParseScreenshotOutputWithName()
{
    const auto r = parseCommand(QStringLiteral("screenshot"),
                                { QStringLiteral("output"), QStringLiteral("eDP-1") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::ScreenshotOutput);
    QCOMPARE(r.outputName, QStringLiteral("eDP-1"));
    QVERIFY(r.filePath.isEmpty());
}

void TreelandDebugTest::testParseScreenshotOutputWithNameAndFile()
{
    const auto r = parseCommand(QStringLiteral("screenshot"),
                                { QStringLiteral("output"), QStringLiteral("eDP-1"),
                                  QStringLiteral("/tmp/cap.png") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::ScreenshotOutput);
    QCOMPARE(r.outputName, QStringLiteral("eDP-1"));
    QCOMPARE(r.filePath, QStringLiteral("/tmp/cap.png"));
}

void TreelandDebugTest::testParseScreenshotOutputEmptyName()
{
    // An explicit empty output name is treated the same as omitting it.
    const auto r = parseCommand(QStringLiteral("screenshot"),
                                { QStringLiteral("output"), QStringLiteral("") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::ScreenshotOutput);
    QVERIFY(r.outputName.isEmpty());
}

void TreelandDebugTest::testParseScreenshotWindowTooFewArgs()
{
    const auto r = parseCommand(QStringLiteral("screenshot"), { QStringLiteral("window") });
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("screenshot window: usage: screenshot window <id> [file]"));
}

void TreelandDebugTest::testParseScreenshotWindow()
{
    const auto r = parseCommand(QStringLiteral("screenshot"),
                                { QStringLiteral("window"), QStringLiteral("42") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::ScreenshotWindow);
    QCOMPARE(r.target, QStringLiteral("42"));
    QVERIFY(r.filePath.isEmpty());
}

void TreelandDebugTest::testParseScreenshotWindowWithFile()
{
    const auto r = parseCommand(QStringLiteral("screenshot"),
                                { QStringLiteral("window"), QStringLiteral("42"),
                                  QStringLiteral("/tmp/win.png") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::ScreenshotWindow);
    QCOMPARE(r.target, QStringLiteral("42"));
    QCOMPARE(r.filePath, QStringLiteral("/tmp/win.png"));
}

// ---------------------------------------------------------------------------
// parseCommand: live commands (top/events/watch)
// ---------------------------------------------------------------------------

void TreelandDebugTest::testParseTopDefault()
{
    const auto r = parseCommand(QStringLiteral("top"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Top);
    QCOMPARE(r.intervalMs, 1000);
}

void TreelandDebugTest::testParseTopCustomInterval()
{
    const auto r = parseCommand(QStringLiteral("top"), { QStringLiteral("500") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Top);
    QCOMPARE(r.intervalMs, 500);
}

void TreelandDebugTest::testParseTopInvalidInterval()
{
    // A non-positive interval falls back to the default 1000 ms.
    const auto r0 = parseCommand(QStringLiteral("top"), { QStringLiteral("0") });
    QVERIFY(r0.ok);
    QCOMPARE(r0.intervalMs, 1000);

    const auto rNeg = parseCommand(QStringLiteral("top"), { QStringLiteral("-5") });
    QVERIFY(rNeg.ok);
    QCOMPARE(rNeg.intervalMs, 1000);
}

void TreelandDebugTest::testParseEventsDefault()
{
    const auto r = parseCommand(QStringLiteral("events"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Events);
    QCOMPARE(r.intervalMs, 50);
}

void TreelandDebugTest::testParseEventsCustomInterval()
{
    const auto r = parseCommand(QStringLiteral("events"), { QStringLiteral("25") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Events);
    QCOMPARE(r.intervalMs, 25);
}

void TreelandDebugTest::testParseEventsInvalidInterval()
{
    const auto r0 = parseCommand(QStringLiteral("events"), { QStringLiteral("0") });
    QVERIFY(r0.ok);
    QCOMPARE(r0.intervalMs, 50);

    const auto rNeg = parseCommand(QStringLiteral("events"), { QStringLiteral("-1") });
    QVERIFY(rNeg.ok);
    QCOMPARE(rNeg.intervalMs, 50);
}

void TreelandDebugTest::testParseWatchMissingId()
{
    const auto r = parseCommand(QStringLiteral("watch"), {});
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("watch: usage: watch <id> [interval-ms]"));
}

void TreelandDebugTest::testParseWatch()
{
    const auto r = parseCommand(QStringLiteral("watch"), { QStringLiteral("42") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Watch);
    QCOMPARE(r.target, QStringLiteral("42"));
    QCOMPARE(r.intervalMs, 250); // default
}

void TreelandDebugTest::testParseWatchWithInterval()
{
    const auto r = parseCommand(QStringLiteral("watch"),
                                { QStringLiteral("42"), QStringLiteral("500") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Watch);
    QCOMPARE(r.target, QStringLiteral("42"));
    QCOMPARE(r.intervalMs, 500);
}

void TreelandDebugTest::testParseWatchInvalidInterval()
{
    const auto r = parseCommand(QStringLiteral("watch"),
                                { QStringLiteral("42"), QStringLiteral("0") });
    QVERIFY(r.ok);
    QCOMPARE(r.target, QStringLiteral("42"));
    QCOMPARE(r.intervalMs, 250); // falls back to default

    const auto rNeg = parseCommand(QStringLiteral("watch"),
                                   { QStringLiteral("42"), QStringLiteral("-100") });
    QVERIFY(rNeg.ok);
    QCOMPARE(rNeg.intervalMs, 250);
}

// ---------------------------------------------------------------------------
// parseCommand: unknown command
// ---------------------------------------------------------------------------

void TreelandDebugTest::testParseUnknownCommand()
{
    const auto r = parseCommand(QStringLiteral("foobar"), {});
    QVERIFY(!r.ok);
    QCOMPARE(r.error, QStringLiteral("unknown command 'foobar' (try --help)"));
    QCOMPARE(r.command, DebugCommand::Unknown);
}

// --- parseCommand: listen ---

void TreelandDebugTest::testParseListenDefault()
{
    const auto r = parseCommand(QStringLiteral("listen"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Listen);
    QCOMPARE(r.port, 8080);
    QCOMPARE(r.host, QStringLiteral("0.0.0.0"));
}

void TreelandDebugTest::testParseListenCustomPort()
{
    const auto r = parseCommand(QStringLiteral("listen"), {QStringLiteral("--port"), QStringLiteral("9090")});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Listen);
    QCOMPARE(r.port, 9090);
    QCOMPARE(r.host, QStringLiteral("0.0.0.0"));
}

void TreelandDebugTest::testParseListenCustomHost()
{
    const auto r = parseCommand(QStringLiteral("listen"), {QStringLiteral("--host"), QStringLiteral("127.0.0.1")});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Listen);
    QCOMPARE(r.port, 8080);
    QCOMPARE(r.host, QStringLiteral("127.0.0.1"));
}

void TreelandDebugTest::testParseListenCustomBoth()
{
    const auto r = parseCommand(QStringLiteral("listen"),
                                {QStringLiteral("--port"), QStringLiteral("3000"),
                                 QStringLiteral("--host"), QStringLiteral("localhost")});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Listen);
    QCOMPARE(r.port, 3000);
    QCOMPARE(r.host, QStringLiteral("localhost"));
}

void TreelandDebugTest::testParseListenInvalidPortLow()
{
    const auto r = parseCommand(QStringLiteral("listen"), {QStringLiteral("--port"), QStringLiteral("0")});
    QVERIFY(!r.ok);
    QVERIFY(r.error.contains(QStringLiteral("port")));
}

void TreelandDebugTest::testParseListenInvalidPortHigh()
{
    const auto r = parseCommand(QStringLiteral("listen"), {QStringLiteral("--port"), QStringLiteral("70000")});
    QVERIFY(!r.ok);
    QVERIFY(r.error.contains(QStringLiteral("port")));
}

void TreelandDebugTest::testParseListenUnknownArg()
{
    const auto r = parseCommand(QStringLiteral("listen"), {QStringLiteral("--foo")});
    QVERIFY(!r.ok);
    QVERIFY(r.error.contains(QStringLiteral("usage")));
}

void TreelandDebugTest::testParseListenPortNoValue()
{
    const auto r = parseCommand(QStringLiteral("listen"), {QStringLiteral("--port")});
    QVERIFY(!r.ok);
    QVERIFY(r.error.contains(QStringLiteral("usage")));
}

QTEST_MAIN(TreelandDebugTest)
#include "main.moc"
