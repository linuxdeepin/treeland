// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "debughelpers.h"

#include "treelanddebugsocket.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QSet>
#include <QPointF>
#include <QRectF>
#include <QTemporaryDir>
#include <QTest>
#include <QLocalServer>
#include <QLocalSocket>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>

// Unit tests for the pure-logic helpers shared by the treeland-debug tool
// (stateName, buttonCode, keyCode, saveCapture, pointToJson, rectToJson)
// plus exhaustive coverage of parseCommand() — the command recognition and
// parameter validation layer that every subcommand flows through.  These do
// not require a running compositor or a Wayland connection.
class TreelandDebugTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    // --- pure helpers ---
    void testStateNameKnownStates();
    void testFormatDuration();
    void testOutputsLabel();
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
    void testRectsToJson();
    void testFormatRegionEmpty();
    void testFormatRegionAtLimit();
    void testFormatRegionOverLimit();

    // --- treeland-debug socket naming / conflict avoidance ---
    void testSocketDefaultPath();
    void testDevelopmentSocketDirectoryUsesUidTemp();
    void testSocketDirs();
    void testSocketFallsBackWhenGlobalDirectoryIsNotSearchable();
    void testSocketPickFreeName();
    void testSocketPickFreeNameOccupied();
    void testSocketPickFreeNameSkipsLiveOldInstance();
    void testSocketCandidateUrls();
    void testSocketPathMatchesQt();
    void testSocketPathForUrl();
    void testSocketUrlFromEnv();
    void testSocketPublished();
    void testPublishedSocketPaths();
    void testSocketPeerText();
    void testSocketPeerTextSanitized();
    void testInstanceSocketUrls();
    void testExistingSocketUrls();
    void testSocketAccessSwitch();

    // --- parseCommand: no-argument commands ---
    void testParseTree();
    void testParseCursor();
    void testParseWindows();
    void testParseClients();
    void testParseIdle();
    void testParseShortcuts();
    void testParseHelp();
    void testParseShell();
    void testParseNoAuth();

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

private:
    // Only used when the machine's XDG_RUNTIME_DIR is not writable: the socket
    // directory resolution reads it once per process.
    QTemporaryDir m_runtimeDir;
    // The global service's own debug directory; a real one is /run/treeland-debug
    // and needs root, so the tests point $TREELAND_DEBUG_DIR here.
    QTemporaryDir m_debugDir;
};

// ---------------------------------------------------------------------------
// Pure-helper tests (unchanged from original coverage)
// ---------------------------------------------------------------------------

void TreelandDebugTest::testStateNameKnownStates()
{
    QCOMPARE(stateName(0), QStringLiteral("Normal"));
    QCOMPARE(stateName(1), QStringLiteral("Maximized"));
    QCOMPARE(stateName(2), QStringLiteral("Fullscreen"));
    QCOMPARE(stateName(3), QStringLiteral("Tiling"));
}

void TreelandDebugTest::testFormatDuration()
{
    QCOMPARE(formatDuration(0), QStringLiteral("0.0s"));
    QCOMPARE(formatDuration(4200), QStringLiteral("4.2s"));
    // 59.95s+ rounds up into the minute format instead of "60.0s".
    QCOMPARE(formatDuration(59949), QStringLiteral("59.9s"));
    QCOMPARE(formatDuration(59999), QStringLiteral("1m00s"));
    QCOMPARE(formatDuration(61000), QStringLiteral("1m01s"));
    QCOMPARE(formatDuration(3599000), QStringLiteral("59m59s"));
    QCOMPARE(formatDuration(3599999), QStringLiteral("1h00m"));
    QCOMPARE(formatDuration(3661000), QStringLiteral("1h01m"));
}

void TreelandDebugTest::testOutputsLabel()
{
    // The frame-pacing output is marked with '+', which cannot be confused
    // with the '*' these views use for the active window.
    QCOMPARE(outputsLabel({ QStringLiteral("eDP-1") }, QStringLiteral("eDP-1"), QStringLiteral("eDP-1")),
             QStringLiteral("+eDP-1"));
    QCOMPARE(outputsLabel({ QStringLiteral("eDP-1"), QStringLiteral("HDMI-A-1") },
                          QStringLiteral("HDMI-A-1"), QStringLiteral("eDP-1")),
             QStringLiteral("eDP-1,+HDMI-A-1"));
    // No known outputs: fall back to the owner output the compositor picked.
    QCOMPARE(outputsLabel({}, QStringLiteral("HDMI-A-1"), QStringLiteral("eDP-1")),
             QStringLiteral("eDP-1"));
    // Pacing output outside the list: nothing is marked.
    QCOMPARE(outputsLabel({ QStringLiteral("eDP-1"), QStringLiteral("HDMI-A-1") },
                          QStringLiteral("DP-1"), QStringLiteral("eDP-1")),
             QStringLiteral("eDP-1,HDMI-A-1"));
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
    // keyCode() returns Linux evdev keycodes (KEY_ESC, KEY_ENTER, ...).
    QCOMPARE(keyCode(QStringLiteral("Escape"), &ok), 1);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("Return"), &ok), 28);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("Space"), &ok), 57);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("Up"), &ok), 103);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("F12"), &ok), 88);
    QVERIFY(ok);
    // Regression: a letter must map to its own keycode, not to whatever key the
    // old reverse-keymap scan happened to visit first.
    QCOMPARE(keyCode(QStringLiteral("a"), &ok), 30); // KEY_A
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("VolumeMute"), &ok), 113); // KEY_MUTE
    QVERIFY(ok);
}

void TreelandDebugTest::testKeyCodeRaw()
{
    bool ok = false;
    QCOMPARE(keyCode(QStringLiteral("200"), &ok), 200);
    QVERIFY(ok);
    // A single digit is a Qt::Key name, not evdev KEY_ESC..KEY_8.
    QCOMPARE(keyCode(QStringLiteral("5"), &ok), 6); // KEY_5
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("0"), &ok), 11); // KEY_0
    QVERIFY(ok);
    // Surrounding whitespace must not fall through to the numeric branch.
    QCOMPARE(keyCode(QStringLiteral(" 5 "), &ok), 6); // KEY_5
    QVERIFY(ok);
    // Out of the range a keymap can carry.
    ok = true;
    QCOMPARE(keyCode(QStringLiteral("300"), &ok), 0);
    QVERIFY(!ok);
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
    // Case-insensitive matching: "ESCAPE" and "enter" should also work.
    QCOMPARE(keyCode(QStringLiteral("ESCAPE"), &ok), 1);
    QVERIFY(ok);
    QCOMPARE(keyCode(QStringLiteral("enter"), &ok), 96); // keypad enter
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

void TreelandDebugTest::testRectsToJson()
{
    // A region is a set of boxes, so it must serialise as an array of rects
    // and an empty region as an empty array.
    QVERIFY(rectsToJson(QRegion()).isEmpty());

    const auto array = rectsToJson(QRegion(0, 0, 8, 8) + QRegion(20, 8, 4, 2));
    QCOMPARE(array.size(), 2);
    const auto first = array.at(0).toObject();
    QCOMPARE(first.value("x").toDouble(), 0.0);
    QCOMPARE(first.value("width").toDouble(), 8.0);
    const auto second = array.at(1).toObject();
    QCOMPARE(second.value("x").toDouble(), 20.0);
    QCOMPARE(second.value("y").toDouble(), 8.0);
    QCOMPARE(second.value("height").toDouble(), 2.0);
}

void TreelandDebugTest::testFormatRegionEmpty()
{
    // An empty region is not "no boxes to show", it is a meaningful state
    // (input: accepts nothing; opaque: no opaque area declared), so it must not
    // render as an empty string.
    QCOMPARE(formatRegion(QRegion()), QStringLiteral("(empty)"));
}

void TreelandDebugTest::testFormatRegionAtLimit()
{
    QRegion region;
    for (int i = 0; i < 8; ++i)
        region += QRect(0, i * 2, 10, 1);
    const QString text = formatRegion(region);
    QCOMPARE(text.count(QLatin1Char('[')), 8);
    QVERIFY(!text.contains(QLatin1String("more")));
    QVERIFY(text.startsWith(QLatin1String("[0,0 10x1],[0,2 10x1]")));
}

void TreelandDebugTest::testFormatRegionOverLimit()
{
    QRegion region;
    for (int i = 0; i < 9; ++i)
        region += QRect(0, i * 2, 10, 1);
    const QString text = formatRegion(region);
    QCOMPARE(text.count(QLatin1Char('[')), 8);
    QVERIFY(text.endsWith(QLatin1String("(+1 more)")));
}

// ---------------------------------------------------------------------------
// treeland-debug socket naming / conflict avoidance
// ---------------------------------------------------------------------------

void TreelandDebugTest::initTestCase()
{
    // Keep socket tests off the machine's live debug directories.
    QVERIFY(m_debugDir.isValid());
    QVERIFY(qputenv("TREELAND_DEBUG_DIR", m_debugDir.path().toLocal8Bit()));

    if (treelandDebugSocketDir().isEmpty()) {
        QVERIFY(m_runtimeDir.isValid());
        QVERIFY(qputenv("XDG_RUNTIME_DIR", m_runtimeDir.path().toLocal8Bit()));
    }
    QVERIFY(!treelandDebugSocketDir().isEmpty());
}

void TreelandDebugTest::testSocketDefaultPath()
{
    // The socket is an absolute path in its selected dedicated directory, not a
    // bare name resolved against the client's potentially different TMPDIR.
    const QString dir = treelandDebugSocketDir();
    const QString name = treelandDebugDefaultSocketName();
    QVERIFY(!name.isEmpty());
    const QString path = dir + QLatin1Char('/') + name;
    QCOMPARE(QFileInfo(path).absolutePath(), dir);
    QCOMPARE(treelandDebugSocketPath(path), path);
    QCOMPARE(treelandDebugSocketPathForUrl(QStringLiteral("local:") + path), path);
}

void TreelandDebugTest::testDevelopmentSocketDirectoryUsesUidTemp()
{
#ifdef TREELAND_DEBUG_DEV_BUILD
    const QByteArray savedOverride = qgetenv("TREELAND_DEBUG_DIR");
    qunsetenv("TREELAND_DEBUG_DIR");
    const QString expected =
        QStringLiteral("/tmp/treeland-debug-%1").arg(static_cast<qulonglong>(::geteuid()));
    const QString dir = treelandDebugSocketDir();
    const QStringList dirs = treelandDebugSocketDirs();
    const QStringList urls = treelandDebugCandidateSocketUrls();
    struct stat st {};
    const bool statOk = ::lstat(QFile::encodeName(expected).constData(), &st) == 0;
    const QString socketPath =
        expected + QStringLiteral("/dev-access-test-%1").arg(::getpid());
    QFile socket(socketPath);
    const bool fileCreated = socket.open(QIODevice::WriteOnly);
    if (fileCreated)
        socket.close();
    const bool opened = fileCreated && treelandDebugSetSocketAccess(socketPath, true);
    struct stat socketSt {};
    const bool socketStatOk = ::stat(QFile::encodeName(socketPath).constData(), &socketSt) == 0;
    struct stat dirSt {};
    const bool dirStatOk = ::stat(QFile::encodeName(expected).constData(), &dirSt) == 0;
    const bool sealed = opened && treelandDebugSetSocketAccess(socketPath, false);
    QFile::remove(socketPath);
    if (savedOverride.isEmpty())
        qunsetenv("TREELAND_DEBUG_DIR");
    else
        qputenv("TREELAND_DEBUG_DIR", savedOverride);

    QCOMPARE(dir, expected);
    QVERIFY(statOk);
    QVERIFY(S_ISDIR(st.st_mode));
    QCOMPARE(st.st_uid, ::geteuid());
    QCOMPARE(st.st_mode & 0777, 0700);
    QVERIFY(fileCreated);
    QVERIFY(opened);
    QVERIFY(socketStatOk);
    QCOMPARE(socketSt.st_mode & 0777, 0600);
    QVERIFY(dirStatOk);
    QCOMPARE(dirSt.st_mode & 0777, 0700);
    QVERIFY(sealed);
    QCOMPARE(dirs.first(), expected);
    QVERIFY(urls.first().startsWith(QStringLiteral("local:") + expected + QLatin1Char('/')));
    QVERIFY(urls.first().endsWith(treelandDebugDefaultSocketName()));
    QVERIFY(urls.last().endsWith(treelandDebugReleaseSocketName()));
#else
    QSKIP("Development socket path is only used by Debug builds");
#endif
}

void TreelandDebugTest::testSocketFallsBackWhenGlobalDirectoryIsNotSearchable()
{
    const QByteArray savedMode = qgetenv("TREELAND_RUN_MODE");
    const QByteArray savedRuntime = qgetenv("XDG_RUNTIME_DIR");
    const QFile::Permissions originalPermissions = QFileInfo(m_debugDir.path()).permissions();
    QVERIFY(m_runtimeDir.isValid());
    QVERIFY(qputenv("XDG_RUNTIME_DIR", m_runtimeDir.path().toLocal8Bit()));
    QVERIFY(QFile::setPermissions(m_debugDir.path(),
                                  QFile::ReadOwner | QFile::WriteOwner));
    qunsetenv("TREELAND_RUN_MODE");

    // The fixture has owner read/write but no execute bits. Check the mode as
    // well as QFileInfo's effective access result, which differs when run as root.
    const QString selectedDir = treelandDebugSocketDir();

    QVERIFY(QFile::setPermissions(m_debugDir.path(), originalPermissions));
    if (savedRuntime.isEmpty())
        qunsetenv("XDG_RUNTIME_DIR");
    else
        qputenv("XDG_RUNTIME_DIR", savedRuntime);
    if (savedMode.isEmpty())
        qunsetenv("TREELAND_RUN_MODE");
    else
        qputenv("TREELAND_RUN_MODE", savedMode);

    QCOMPARE(selectedDir, m_runtimeDir.path());

    QVERIFY(QFile::setPermissions(m_debugDir.path(), originalPermissions));
    if (savedRuntime.isEmpty())
        qunsetenv("XDG_RUNTIME_DIR");
    else
        qputenv("XDG_RUNTIME_DIR", savedRuntime);
    if (savedMode.isEmpty())
        qunsetenv("TREELAND_RUN_MODE");
    else
        qputenv("TREELAND_RUN_MODE", savedMode);
}
void TreelandDebugTest::testSocketDirs()
{
    const QByteArray savedRuntime = qgetenv("XDG_RUNTIME_DIR");
    QVERIFY(m_runtimeDir.isValid());
    QVERIFY(qputenv("XDG_RUNTIME_DIR", m_runtimeDir.path().toLocal8Bit()));

    // This process' own dev/runtime directories come first, then the shared
    // global and legacy locations.
    const QStringList dirs = treelandDebugSocketDirs();
    QVERIFY(!dirs.isEmpty());
    QCOMPARE(dirs.first(), treelandDebugSocketDir());
    QCOMPARE(dirs.first(), m_debugDir.path());
    QVERIFY(dirs.contains(treelandDebugGlobalSocketDir()));
    QVERIFY(dirs.contains(QStringLiteral("/run/treeland")));
    QVERIFY(QFileInfo(treelandDebugSocketDir()).isDir());
    QCOMPARE(dirs.size(), QSet<QString>(dirs.begin(), dirs.end()).size());
    for (const QString &dir : dirs)
        QVERIFY(dir.startsWith(QLatin1Char('/')));
#ifdef TREELAND_DEBUG_DEV_BUILD
    QVERIFY(dirs.contains(m_runtimeDir.path()));
    for (const QString &dir : dirs) {
        if (dir.startsWith(QStringLiteral("/run/user/")))
            QVERIFY(dir == m_runtimeDir.path());
    }
#endif

    if (savedRuntime.isEmpty())
        qunsetenv("XDG_RUNTIME_DIR");
    else
        qputenv("XDG_RUNTIME_DIR", savedRuntime);

    // A session instance uses its own runtime directory; Qt's temp fallback is
    // rejected because publishing there would put the socket back in /tmp.
    const QByteArray savedMode = qgetenv("TREELAND_RUN_MODE");
    qputenv("TREELAND_RUN_MODE", "user");
    QCOMPARE(treelandDebugSocketDir(), savedRuntime);
    qunsetenv("XDG_RUNTIME_DIR");
    QVERIFY(treelandDebugSocketDir().isEmpty());
    QVERIFY(qputenv("XDG_RUNTIME_DIR", savedRuntime));
    QCOMPARE(treelandDebugSocketDir(), savedRuntime);
    if (savedMode.isEmpty())
        qunsetenv("TREELAND_RUN_MODE");
    else
        qputenv("TREELAND_RUN_MODE", savedMode);
    QCOMPARE(treelandDebugSocketDir(), m_debugDir.path());
}

void TreelandDebugTest::testSocketPickFreeName()
{
    // Free base → the base path itself, lock held.  Use a PID-qualified name so
    // parallel ctest runs don't collide.
    const QString name = QStringLiteral("test.treeland.debug.socket.free.%1").arg(::getpid());
    const QString base = treelandDebugSocketDir() + QLatin1Char('/') + name;
    QLocalServer::removeServer(base);

    TreelandDebugSocketLock lock;
    QCOMPARE(treelandDebugPickFreeSocketName(lock, base), base);
    QVERIFY(lock.isHeld());
    // Lock released on scope exit → lock file unlinked.

    // A bare name is not a socket path any more; the caller must resolve it.
    TreelandDebugSocketLock bareLock;
    QVERIFY(treelandDebugPickFreeSocketName(bareLock, name).isEmpty());
    QVERIFY(!bareLock.isHeld());
}

void TreelandDebugTest::testSocketPickFreeNameOccupied()
{
    // Occupied base → numeric suffix, and the occupied name is untouched.
    const QString name = QStringLiteral("test.treeland.debug.socket.occupied.%1").arg(::getpid());
    const QString base = treelandDebugSocketDir() + QLatin1Char('/') + name;
    const QString suffix = base + QStringLiteral("-1");
    QLocalServer::removeServer(base);
    QLocalServer::removeServer(suffix);

    // Occupy the base name by holding its lock.
    TreelandDebugSocketLock holder;
    QVERIFY(holder.tryAcquire(base));

    TreelandDebugSocketLock lock;
    QCOMPARE(treelandDebugPickFreeSocketName(lock, base), suffix);
    QVERIFY(lock.isHeld());
    // Both locks released on scope exit → lock files unlinked.
}

void TreelandDebugTest::testSocketPickFreeNameSkipsLiveOldInstance()
{
    // An instance may listen on the base socket WITHOUT holding a flock lock
    // file. The picker must not clobber it (removeServer would delete the live
    // socket path) — it should skip the live name and take a suffix instead.
    const QString name = QStringLiteral("test.treeland.debug.socket.live.%1").arg(::getpid());
    const QString base = treelandDebugSocketDir() + QLatin1Char('/') + name;
    QLocalServer::removeServer(base);

    QLocalServer server;
    server.setSocketOptions(QLocalServer::UserAccessOption);
    QVERIFY(server.listen(base));
    // Discard the brief probe connections so they don't queue up.
    connect(&server, &QLocalServer::newConnection, &server,
            [&server]() { delete server.nextPendingConnection(); });

    TreelandDebugSocketLock lock;
    // base is live (server listening) but has no lock file -> must skip.
    QCOMPARE(treelandDebugPickFreeSocketName(lock, base),
             base + QStringLiteral("-1"));
    QVERIFY(lock.isHeld());

    server.close();
    QLocalServer::removeServer(base);
}

void TreelandDebugTest::testSocketCandidateUrls()
{
    // Debug builds probe the dev socket in every directory first, then preserve
    // the existing release fallback. Release builds only probe release sockets.
    const auto urls = treelandDebugCandidateSocketUrls();
    QVERIFY(!urls.isEmpty());

    // Every candidate must be a local: URL with an absolute path, so it does
    // not depend on the client's temp or runtime directory.
    const QStringList dirs = treelandDebugSocketDirs();
    for (const auto &u : urls) {
        QVERIFY(u.startsWith(QStringLiteral("local:/")));
        bool inKnownDir = false;
        for (const auto &dir : dirs)
            inKnownDir = inKnownDir || u.startsWith(QStringLiteral("local:") + dir + QLatin1Char('/'));
        QVERIFY(inKnownDir);
    }

#ifdef TREELAND_DEBUG_DEV_BUILD
    // Debug build: the dev socket in every directory, then the release socket.
    QCOMPARE(urls.size(), 2 * dirs.size());
    QVERIFY(urls.at(0).endsWith(treelandDebugDefaultSocketName()));
    QVERIFY(urls.at(0).contains(QStringLiteral("debug-dev")));
    QVERIFY(urls.last().endsWith(treelandDebugReleaseSocketName()));
#else
    // Release build: only the release socket.
    QCOMPARE(urls.size(), dirs.size());
    QVERIFY(urls.first().endsWith(treelandDebugReleaseSocketName()));
#endif
}

void TreelandDebugTest::testSocketPathMatchesQt()
{
    // Our own URLs carry the absolute path, which Qt uses as-is -- so the
    // chmod() in release builds hits the socket the host actually bound.
    const QString name = QStringLiteral("test.treeland.debug.socket.path.%1").arg(::getpid());
    const QString path = treelandDebugSocketDir() + QLatin1Char('/') + name;
    QLocalServer::removeServer(path);
    QLocalServer server;
    QVERIFY(server.listen(path));
    QCOMPARE(treelandDebugSocketPath(path), server.fullServerName());
    QVERIFY(treelandDebugSocketPublished(QStringLiteral("local:") + path));
    server.close();
    QLocalServer::removeServer(path);

    // A hand-written bare name keeps Qt's rule (QDir::tempPath()), so the
    // diagnostics inspect the same file Qt would connect to.
    QLocalServer::removeServer(name);
    QLocalServer bareServer;
    QVERIFY(bareServer.listen(name));
    QCOMPARE(treelandDebugSocketPath(name), bareServer.fullServerName());
    QCOMPARE(treelandDebugSocketPath(name),
             QDir::cleanPath(QDir::tempPath()) + QLatin1Char('/') + name);
    bareServer.close();
    QLocalServer::removeServer(name);
}

void TreelandDebugTest::testSocketPathForUrl()
{
    // A bare name resolves like Qt does (temp dir); absolute paths are kept,
    // non-local URLs have no socket file.
    const QString bare = treelandDebugSocketPathForUrl(QStringLiteral("local:foo.debug"));
    QCOMPARE(bare, QDir::cleanPath(QDir::tempPath()) + QStringLiteral("/foo.debug"));
    QCOMPARE(treelandDebugSocketPathForUrl(QStringLiteral("local:/run/foo.debug")),
             QStringLiteral("/run/foo.debug"));
    QVERIFY(treelandDebugSocketPathForUrl(QStringLiteral("tcp://127.0.0.1:1234")).isEmpty());
    QVERIFY(treelandDebugSocketPathForUrl(QStringLiteral("local:")).isEmpty());
}

void TreelandDebugTest::testSocketUrlFromEnv()
{
    // $TREELAND_DEBUG_SOCKET lets a caller name the socket without a flag (the
    // flag itself wins, the probing below is the last resort): a plain path or a
    // bare name means a local socket, anything with a scheme is used verbatim.
    const QByteArray saved = qgetenv("TREELAND_DEBUG_SOCKET");
    qunsetenv("TREELAND_DEBUG_SOCKET");
    QVERIFY(treelandDebugSocketUrlFromEnv().isEmpty());

    const QString path = treelandDebugSocketDir() + QStringLiteral("/env.sock");
    QVERIFY(qputenv("TREELAND_DEBUG_SOCKET", path.toLocal8Bit()));
    QCOMPARE(treelandDebugSocketUrlFromEnv(), QStringLiteral("local:") + path);

    QVERIFY(qputenv("TREELAND_DEBUG_SOCKET", "env.sock"));
    QCOMPARE(treelandDebugSocketUrlFromEnv(), QStringLiteral("local:env.sock"));

    QVERIFY(qputenv("TREELAND_DEBUG_SOCKET", "local:env.sock"));
    QCOMPARE(treelandDebugSocketUrlFromEnv(), QStringLiteral("local:env.sock"));

    QVERIFY(qputenv("TREELAND_DEBUG_SOCKET", "tcp://127.0.0.1:1234"));
    QCOMPARE(treelandDebugSocketUrlFromEnv(), QStringLiteral("tcp://127.0.0.1:1234"));

    if (saved.isEmpty())
        qunsetenv("TREELAND_DEBUG_SOCKET");
    else
        qputenv("TREELAND_DEBUG_SOCKET", saved);
}

void TreelandDebugTest::testSocketPublished()
{
    // A published socket (or its numeric-suffixed variant) is how the client
    // tells that the debug remote source already exists, so it must not report
    // a missing socket as published. Only real sockets count: a plain file at
    // the name is not a debug channel. PID-qualified names keep parallel ctest
    // runs apart.
    const QString name = QStringLiteral("test.treeland.debug.published.%1").arg(::getpid());
    const QString base = treelandDebugSocketDir() + QLatin1Char('/') + name;
    const QString url = QStringLiteral("local:") + base;
    QLocalServer::removeServer(base);
    QVERIFY(!treelandDebugSocketPublished(url));

    QLocalServer server;
    QVERIFY(server.listen(base));
    QVERIFY(treelandDebugSocketPublished(url));
    server.close();
    QLocalServer::removeServer(base);

    QFile plainFile(base);
    QVERIFY(plainFile.open(QIODevice::WriteOnly));
    plainFile.close();
    QVERIFY(!treelandDebugSocketPublished(url));
    QVERIFY(plainFile.remove());

    QLocalServer suffixed;
    QVERIFY(suffixed.listen(base + QStringLiteral("-1")));
    QVERIFY(treelandDebugSocketPublished(url));
    suffixed.close();
    QLocalServer::removeServer(base + QStringLiteral("-1"));
    QVERIFY(!treelandDebugSocketPublished(url));
}

void TreelandDebugTest::testPublishedSocketPaths()
{
    // The client reports *which* sockets it found, so suffix handling matters:
    // only "<name>-<digits>" belongs to this name, only real sockets count, and
    // several hits must all be reported instead of picking one (that is how the
    // error message ends up naming the wrong reason).
    const QString name = QStringLiteral("test.treeland.debug.paths.%1").arg(::getpid());
    const QString base = treelandDebugSocketDir() + QLatin1Char('/') + name;
    const QString url = QStringLiteral("local:") + base;

    QVERIFY(treelandDebugPublishedSocketPaths(url).isEmpty());

    QLocalServer exact;
    QVERIFY(exact.listen(base));
    QCOMPARE(treelandDebugPublishedSocketPaths(url), QStringList{ base });
    exact.close();
    QLocalServer::removeServer(base);
    QVERIFY(treelandDebugPublishedSocketPaths(url).isEmpty());

    // Several numeric siblings are all reported, whatever order the directory
    // returns them in.
    QLocalServer second;
    QLocalServer tenth;
    QVERIFY(second.listen(base + QStringLiteral("-2")));
    QVERIFY(tenth.listen(base + QStringLiteral("-10")));
    QStringList paths = treelandDebugPublishedSocketPaths(url);
    paths.sort();
    QStringList expected{ base + QStringLiteral("-10"), base + QStringLiteral("-2") };
    expected.sort();
    QCOMPARE(paths, expected);

    // The other build type's socket ("<name>-dev"), non-numeric suffixes and
    // plain files are not this name's sockets.
    QLocalServer otherBuild;
    QVERIFY(otherBuild.listen(base + QStringLiteral("-dev")));
    QFile plainFile(base + QStringLiteral("-1.bak"));
    QVERIFY(plainFile.open(QIODevice::WriteOnly));
    plainFile.close();
    QFile trailingGarbage(base + QStringLiteral("-1bak"));
    QVERIFY(trailingGarbage.open(QIODevice::WriteOnly));
    trailingGarbage.close();
    paths = treelandDebugPublishedSocketPaths(url);
    paths.sort();
    QCOMPARE(paths, expected);

    second.close();
    tenth.close();
    otherBuild.close();
    QLocalServer::removeServer(base + QStringLiteral("-2"));
    QLocalServer::removeServer(base + QStringLiteral("-10"));
    QLocalServer::removeServer(base + QStringLiteral("-dev"));
    QVERIFY(QFile::remove(base + QStringLiteral("-1.bak")));
    QVERIFY(QFile::remove(base + QStringLiteral("-1bak")));
    QVERIFY(treelandDebugPublishedSocketPaths(url).isEmpty());
}

void TreelandDebugTest::testSocketPeerText()
{
    // A connect failure that reached the socket names the process on the other
    // end of it, from the kernel's SO_PEERCRED -- not this process's own
    // credentials.  The listener therefore lives in a child process: an
    // implementation that reported its own pid/uid would fail here.
    const QString name = QStringLiteral("test.treeland.debug.peer.%1").arg(::getpid());
    const QByteArray path = QFile::encodeName(treelandDebugSocketPath(name));
    sockaddr_un address {};
    address.sun_family = AF_UNIX;
    QVERIFY(path.size() < static_cast<int>(sizeof(address.sun_path)));
    ::memcpy(address.sun_path, path.constData(), size_t(path.size()));

    // fork() rather than a helper binary: the child only makes raw socket calls
    // and _exit()s, so no Qt state is touched on that side of the fork.
    const pid_t child = ::fork();
    QVERIFY(child >= 0);
    if (child == 0) {
        const int listener = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (listener < 0)
            ::_exit(1);
        ::unlink(address.sun_path);
        if (::bind(listener, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0
            || ::listen(listener, 1) != 0) {
            ::_exit(1);
        }
        ::accept(listener, nullptr, nullptr);   // the parent connects next
        ::_exit(0);
    }

    QLocalSocket client;
    client.connectToServer(name);
    const bool connected = client.waitForConnected(2000);

    const QString text = treelandDebugSocketPeerText(client);
    if (connected) {
        QVERIFY2(text.startsWith(QStringLiteral("pid %1 ").arg(child)), qPrintable(text));
        QVERIFY2(!text.startsWith(QStringLiteral("pid %1 ").arg(::getpid())), qPrintable(text));
        QVERIFY2(text.contains(QStringLiteral("uid %1").arg(::getuid())), qPrintable(text));
    } else {
        QVERIFY(!treelandDebugSocketPeerText(client).startsWith(
            QStringLiteral("pid %1 ").arg(::getpid())));
    }

    // A socket that is not connected has no peer to report.
    QLocalSocket idle;
    QVERIFY(treelandDebugSocketPeerText(idle).isEmpty());

    client.close();
    ::kill(child, SIGKILL);
    ::waitpid(child, nullptr, 0);
    ::unlink(address.sun_path);
}

void TreelandDebugTest::testSocketPeerTextSanitized()
{
    // The peer names itself: control bytes in /proc/<pid>/comm must not reach the
    // terminal through the diagnostic, or a process owning the socket could
    // rewrite it. This process is a convenient peer to impersonate.
    QFile rename(QStringLiteral("/proc/self/comm"));
    if (!rename.open(QIODevice::ReadOnly))
        QSKIP("cannot read this process's name");
    const QByteArray original = rename.readAll().trimmed();
    rename.close();
    if (!rename.open(QIODevice::WriteOnly))
        QSKIP("cannot rename this process");
    rename.write(QByteArray("ev\x1b[2Jil\x7f\x9b"));
    rename.close();

    const QString name = QStringLiteral("test.treeland.debug.peer.evil.%1").arg(::getpid());
    QLocalServer::removeServer(name);
    QLocalServer server;
    QVERIFY(server.listen(name));
    QLocalSocket client;
    client.connectToServer(name);
    QVERIFY(client.waitForConnected(1000));

    const QString text = treelandDebugSocketPeerText(client);
    QVERIFY2(!text.contains(QChar(0x1b)), qPrintable(text));
    QVERIFY2(!text.contains(QChar(0x7f)), qPrintable(text));
    // The C1 block: a byte 0x9b becomes U+009B through fromLatin1(), which
    // terminals that decode C1 still act on.
    QVERIFY2(!text.contains(QChar(0x9b)), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("(ev?[2Jil\?\?)")), qPrintable(text));

    client.close();
    server.close();
    QLocalServer::removeServer(name);

    QFile restore(QStringLiteral("/proc/self/comm"));
    if (restore.open(QIODevice::WriteOnly))
        restore.write(original);
}

void TreelandDebugTest::testInstanceSocketUrls()
{
    // A treeland that finds its base name taken publishes "<name>-<N>". The
    // client must try those too, keeping the base preference order, or a foreign
    // socket on the base name makes it report a failure while an instance of its
    // own build is answering.
    const QString name = QStringLiteral("test.treeland.debug.urls.%1").arg(::getpid());
    const QString url = QStringLiteral("local:") + name;
    const QString other = QStringLiteral("local:test.treeland.debug.urls.other");

    QLocalServer::removeServer(name);
    QCOMPARE(treelandDebugInstanceSocketUrls({ url, other }), (QStringList{ url, other }));

    // The suffixed instance follows its own candidate, so the candidate order
    // (which build type is preferred) still decides before the base name does.
    QLocalServer suffixed;
    QVERIFY(suffixed.listen(name + QStringLiteral("-2")));
    QCOMPARE(treelandDebugInstanceSocketUrls({ url, other }),
             (QStringList{ url, url + QStringLiteral("-2"), other }));

    suffixed.close();
    QLocalServer::removeServer(name + QStringLiteral("-2"));
    QCOMPARE(treelandDebugInstanceSocketUrls({ url, other }), (QStringList{ url, other }));

    // An absolute candidate keeps its directory: a bare name would be resolved
    // under QDir::tempPath() instead, pointing at a different socket.
    QTemporaryDir absoluteDir;
    QVERIFY(absoluteDir.isValid());
    const QString absoluteBase = absoluteDir.filePath(QStringLiteral("abs.debug"));
    QLocalServer absolute;
    QVERIFY(absolute.listen(absoluteBase));
    QLocalServer absoluteSuffixed;
    QVERIFY(absoluteSuffixed.listen(absoluteBase + QStringLiteral("-3")));
    QCOMPARE(treelandDebugInstanceSocketUrls({ QStringLiteral("local:") + absoluteBase }),
             (QStringList{ QStringLiteral("local:") + absoluteBase,
                          QStringLiteral("local:") + absoluteBase + QStringLiteral("-3") }));
    absoluteSuffixed.close();
    absolute.close();
    QLocalServer::removeServer(absoluteBase + QStringLiteral("-3"));
    QLocalServer::removeServer(absoluteBase);
}

void TreelandDebugTest::testExistingSocketUrls()
{
    // Probing a candidate whose socket file is absent can only spend a probe
    // timeout, so callers filter those out -- unless nothing is left, in which case
    // the full list is kept so the failure can name every place that was searched.
    const QString name = QStringLiteral("test.treeland.debug.existing.%1").arg(::getpid());
    const QString url = QStringLiteral("local:") + name;
    const QString absent = QStringLiteral("local:test.treeland.debug.existing.absent");

    QLocalServer::removeServer(name);
    QCOMPARE(treelandDebugExistingSocketUrls({ url, absent }), (QStringList{ url, absent }));

    QLocalServer server;
    QVERIFY(server.listen(name));
    QCOMPARE(treelandDebugExistingSocketUrls({ absent, url }), QStringList{ url });

    server.close();
    QLocalServer::removeServer(name);
    QCOMPARE(treelandDebugExistingSocketUrls({ url, absent }), (QStringList{ url, absent }));
}

void TreelandDebugTest::testSocketAccessSwitch()
{
#ifdef TREELAND_DEBUG_DEV_BUILD
    // A Debug dev socket is owner-only; same-UID access works without polkit.
#else
    // Release no-auth opens only the global service socket and widens its
    // dedicated directory so other local users can reach it.
#endif
    const QString path = treelandDebugSocketDir() + QStringLiteral("/debug.socket");
    QFile socket(path);
    QVERIFY(socket.open(QIODevice::WriteOnly));
    socket.close();
    QCOMPARE(::chmod(QFile::encodeName(path).constData(), 0), 0);
    const QString neighbour = treelandDebugSocketDir() + QStringLiteral("/neighbour.socket");
    QFile neighbourSocket(neighbour);
    QVERIFY(neighbourSocket.open(QIODevice::WriteOnly));
    neighbourSocket.close();
    QCOMPARE(::chmod(QFile::encodeName(neighbour).constData(), 0755), 0);

    const auto modeOf = [](const QString &file) {
        struct stat st {};
        return ::stat(QFile::encodeName(file).constData(), &st) == 0 ? (st.st_mode & 07777) : 0u;
    };

    QVERIFY(treelandDebugSetSocketAccess(path, true));
#ifdef TREELAND_DEBUG_DEV_BUILD
    QCOMPARE(modeOf(path), 0600u);
    QCOMPARE(modeOf(treelandDebugSocketDir()), 0700u);
#else
    QCOMPARE(modeOf(path), 0666u);
    QCOMPARE(modeOf(treelandDebugSocketDir()), 0711u);
#endif

    QVERIFY(treelandDebugSetSocketAccess(path, true));
#ifdef TREELAND_DEBUG_DEV_BUILD
    QCOMPARE(modeOf(treelandDebugSocketDir()), 0700u);
#else
    QCOMPARE(modeOf(treelandDebugSocketDir()), 0711u);
#endif

    QVERIFY(treelandDebugSetSocketAccess(path, false));
    QCOMPARE(modeOf(path), 0u);
#ifdef TREELAND_DEBUG_DEV_BUILD
    QCOMPARE(modeOf(treelandDebugSocketDir()), 0700u);
#else
    QCOMPARE(modeOf(treelandDebugSocketDir()), 0711u);
#endif
    QCOMPARE(modeOf(neighbour), 0755u);
    QFile::remove(path);
    QFile::remove(neighbour);

    QVERIFY(!treelandDebugSetSocketAccess(QStringLiteral("/run/user/0/treeland/x.socket"), true));
    QTemporaryDir elsewhere;
    QVERIFY(elsewhere.isValid());
    const QString foreign = elsewhere.filePath(QStringLiteral("debug.socket"));
    QFile foreignSocket(foreign);
    QVERIFY(foreignSocket.open(QIODevice::WriteOnly));
    foreignSocket.close();
    QCOMPARE(::chmod(QFile::encodeName(foreign).constData(), 0), 0);
    QVERIFY(!treelandDebugSetSocketAccess(foreign, true));
    QCOMPARE(modeOf(foreign), 0u);
    QCOMPARE(modeOf(elsewhere.path()), 0700u);
    QVERIFY(treelandDebugSetSocketAccess(foreign, false));

    // An unsafe sticky/world-writable directory must not be widened.
    QCOMPARE(::chmod(QFile::encodeName(treelandDebugSocketDir()).constData(), 01777), 0);
    QVERIFY(!treelandDebugSetSocketAccess(path, true));
    QCOMPARE(modeOf(treelandDebugSocketDir()), 01777u);
#ifdef TREELAND_DEBUG_DEV_BUILD
    QCOMPARE(::chmod(QFile::encodeName(treelandDebugSocketDir()).constData(), 0700), 0);
#else
    QCOMPARE(::chmod(QFile::encodeName(treelandDebugSocketDir()).constData(), 0711), 0);
#endif
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

void TreelandDebugTest::testParseIdle()
{
    const auto r = parseCommand(QStringLiteral("idle"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Idle);
    QVERIFY(r.error.isEmpty());
}

void TreelandDebugTest::testParseShortcuts()
{
    const auto r = parseCommand(QStringLiteral("shortcuts"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::Shortcuts);
    QCOMPARE(r.intervalMs, 0); // one-shot listing, not a live view
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

void TreelandDebugTest::testParseNoAuth()
{
    // The no-auth window is requested over the protocol; the command that sends
    // it takes no arguments, like the other inspection commands.
    const auto r = parseCommand(QStringLiteral("no-auth"), {});
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::NoAuth);
    QVERIFY(r.error.isEmpty());
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
    QCOMPARE(r.code, 96); // keypad enter
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
                                { QStringLiteral("key"), QStringLiteral("Space"), QStringLiteral("release") });
    QVERIFY(r.ok);
    QCOMPARE(r.command, DebugCommand::EventKey);
    QCOMPARE(r.code, 57); // KEY_SPACE
    QCOMPARE(r.action, QStringLiteral("release"));
}

void TreelandDebugTest::testParseEventKeyDefaultAction()
{
    const auto r = parseCommand(QStringLiteral("event"),
                                { QStringLiteral("key"), QStringLiteral("Escape") });
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
