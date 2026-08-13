// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QRemoteObjectNode>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

#include "rep_treeland_windowtree_replica.h"

namespace {

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

QJsonObject windowToJson(const WindowInfo &window)
{
    return {
        {"id", window.id()},
        {"appId", window.appId()},
        {"title", window.title()},
        {"output", window.output()},
        {"container", window.container()},
        {"workspace", window.workspace()},
        {"layer", window.layer()},
        {"z", window.z()},
        {"type", window.type()},
        {"state", window.state()},
        {"visible", window.visible()},
        {"active", window.active()},
        {"geometry", rectToJson(window.geometry())},
        {"titlebarGeometry", rectToJson(window.titlebarGeometry())},
        {"boundingRect", rectToJson(window.boundingRect())},
        {"iconGeometry", rectToJson(window.iconGeometry())},
        {"position", pointToJson(window.position())},
    };
}

QJsonArray windowsToJson(const QList<WindowInfo> &windows)
{
    QJsonArray result;
    for (const auto &window : windows)
        result.append(windowToJson(window));
    return result;
}

QJsonObject workspaceToJson(const WorkspaceInfo &workspace)
{
    return {
        {"id", workspace.id()},
        {"isActive", workspace.isActive()},
        {"windows", windowsToJson(workspace.windows())},
    };
}

QJsonArray workspacesToJson(const QList<WorkspaceInfo> &workspaces)
{
    QJsonArray result;
    for (const auto &workspace : workspaces)
        result.append(workspaceToJson(workspace));
    return result;
}

QJsonObject layerToJson(const LayerInfo &layer)
{
    return {
        {"name", layer.name()},
        {"layer", layer.layer()},
        {"windows", windowsToJson(layer.windows())},
        {"workspaces", workspacesToJson(layer.workspaces())},
    };
}

QJsonArray layersToJson(const QList<LayerInfo> &layers)
{
    QJsonArray result;
    for (const auto &layer : layers)
        result.append(layerToJson(layer));
    return result;
}

QJsonObject treelandInfoToJson(const TreelandInfo &info)
{
    return {
        {"currentMode", info.currentMode()},
        {"layers", layersToJson(info.layers())},
    };
}

QJsonObject clientToJson(const ClientInfo &client)
{
    return {
        {"id", client.id()},
        {"pid", client.pid()},
        {"executable", client.executable()},
        {"windows", windowsToJson(client.windows())},
    };
}

QJsonArray clientsToJson(const QList<ClientInfo> &clients)
{
    QJsonArray result;
    for (const auto &client : clients)
        result.append(clientToJson(client));
    return result;
}

void registerNamedMetatypes()
{
    WindowTreeRemoteReplica::registerMetatypes();
    qRegisterMetaType<WindowInfo>("WindowInfo");
    qRegisterMetaType<QList<WindowInfo>>("QList<WindowInfo>");
    qRegisterMetaType<WorkspaceInfo>("WorkspaceInfo");
    qRegisterMetaType<QList<WorkspaceInfo>>("QList<WorkspaceInfo>");
    qRegisterMetaType<LayerInfo>("LayerInfo");
    qRegisterMetaType<QList<LayerInfo>>("QList<LayerInfo>");
    qRegisterMetaType<TreelandInfo>("TreelandInfo");
    qRegisterMetaType<ClientInfo>("ClientInfo");
    qRegisterMetaType<QList<ClientInfo>>("QList<ClientInfo>");
}

int fail(const QString &message)
{
    QTextStream(stderr) << "treeland-debug: " << message << Qt::endl;
    return EXIT_FAILURE;
}

void printJson(const QJsonDocument &document)
{
    QTextStream(stdout) << QString::fromUtf8(document.toJson(QJsonDocument::Indented));
}

// A connection to the running Treeland debug Remote Object.
struct Session
{
    QRemoteObjectNode node;
    WindowTreeRemoteReplica *replica = nullptr;
};

bool connectSession(Session &session, const QString &url, const QString &name, int timeoutMs)
{
    if (!session.node.connectToNode(QUrl(url)))
        return false;
    session.replica = session.node.acquire<WindowTreeRemoteReplica>(name);
    return session.replica->waitForSource(timeoutMs);
}

// Waits for a typed replica slot call and stores its return value.
template <typename T>
bool waitSlot(QRemoteObjectPendingReply<T> call, int timeoutMs, T *out)
{
    if (!call.waitForFinished(timeoutMs))
        return false;
    if (call.error() != QRemoteObjectPendingCall::NoError)
        return false;
    if (out)
        *out = call.returnValue();
    return true;
}

// Resolves a window target: a numeric id is used as-is, any other token is
// matched against the first window whose appId equals it.
qint64 resolveTarget(Session &session, int timeoutMs, const QString &token, bool *ok)
{
    *ok = true;
    bool parsed = false;
    const qint64 id = token.toLongLong(&parsed);
    if (parsed)
        return id;

    QList<WindowInfo> windows;
    if (!waitSlot(session.replica->getWindows(), timeoutMs, &windows)) {
        *ok = false;
        return 0;
    }
    for (const auto &window : windows) {
        if (window.appId() == token)
            return window.id();
    }
    *ok = false;
    return 0;
}

// Maps a friendly button name to a Linux input button code.
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

// Maps a friendly key name to a Linux evdev keycode; raw codes pass through.
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

void printWindowsTable(const QList<WindowInfo> &windows)
{
    QTextStream out(stdout);
    out << QStringLiteral("ID              APP-ID                STATE        ACTIVE  OUTPUT   GEOMETRY              TITLE\n");
    for (const auto &window : windows) {
        const auto g = window.geometry();
        const QString line = QStringLiteral("%1  %2  %3  %4  %5  %6,%7 %8x%9  %10")
            .arg(QString::number(window.id()).leftJustified(16))
            .arg(window.appId().leftJustified(22))
            .arg(stateName(window.state()).leftJustified(12))
            .arg(window.active() ? QStringLiteral("yes") : QStringLiteral("no"))
            .arg(window.output().leftJustified(8))
            .arg(static_cast<int>(g.x()))
            .arg(static_cast<int>(g.y()))
            .arg(static_cast<int>(g.width()))
            .arg(static_cast<int>(g.height()))
            .arg(window.title());
        out << line << Qt::endl;
    }
}

void printClientsTable(const QList<ClientInfo> &clients)
{
    QTextStream out(stdout);
    for (const auto &client : clients) {
        out << QStringLiteral("client pid=%1 id=0x%2 %3  (windows: %4)\n")
                .arg(client.pid())
                .arg(client.id(), 0, 16)
                .arg(client.executable())
                .arg(client.windows().size());
        for (const auto &window : client.windows()) {
            const auto g = window.geometry();
            out << QStringLiteral("  %1%2  id=%3  %4  %5,%6 %7x%8  [%9]\n")
                    .arg(window.active() ? QStringLiteral("*") : QStringLiteral(" "))
                    .arg(window.appId().leftJustified(24))
                    .arg(window.id())
                    .arg(stateName(window.state()))
                    .arg(static_cast<int>(g.x()))
                    .arg(static_cast<int>(g.y()))
                    .arg(static_cast<int>(g.width()))
                    .arg(static_cast<int>(g.height()))
                    .arg(window.output());
        }
        out << Qt::endl;
    }
}

QString helpText()
{
    return QStringLiteral(
        "Usage: treeland-debug [global-options] <command> [args]\n"
        "\n"
        "An adb-style inspector and controller for the Treeland compositor. Runs in\n"
        "one-shot (non-shell) mode by default; use the `shell` command for an\n"
        "interactive REPL.\n"
        "\n"
        "Global options:\n"
        "  --url <url>          Remote object host URL (default: local:org.deepin.dde.treeland.debug)\n"
        "  --name <name>        Remote object name (default: WindowTree)\n"
        "  --timeout-ms <n>     Request timeout in milliseconds (default: 30000)\n"
        "  --json               Emit machine-readable JSON for `windows`/`clients`\n"
        "  -h, --help           Show this help\n"
        "  -v, --version        Show version\n"
        "\n"
        "Inspection:\n"
        "  tree                       Print the complete window tree (default)\n"
        "  cursor                     Print the cursor position\n"
        "  windows                    List all toplevel windows (use --json for JSON)\n"
        "  clients                    List connected Wayland clients and their windows\n"
        "  top [interval-ms]          Live, top-like refreshing view (default 1000ms)\n"
        "\n"
        "Window control (target by numeric id or appId):\n"
        "  activate <id>              Activate (focus) a window\n"
        "  close <id>                 Close a window\n"
        "  minimize <id>              Minimize a window\n"
        "  maximize <id>              Toggle maximized state\n"
        "  fullscreen <id>            Toggle fullscreen state\n"
        "  move <id> <x> <y>          Move a window to (x, y)\n"
        "  resize <id> <w> <h>        Resize a window to (w, h)\n"
        "  workspace <id> <ws-id>     Move a window to a workspace\n"
        "\n"
        "Input / event injection:\n"
        "  move-cursor <x> <y>        Move the cursor to (x, y)\n"
        "  event motion <x> <y>       Move the cursor to (x, y)\n"
        "  event button <btn> [act]   Send a pointer button; btn = left|right|middle|<code>,\n"
        "                             act = press|release|click (default click)\n"
        "  event key <key> [act]      Send a keyboard event; key = name|<evdev-code>,\n"
        "                             act = press|release|tap (default tap)\n"
        "\n"
        "Image capture:\n"
        "  screenshot output [name] [file]   Grab an output (name optional, primary by default)\n"
        "  screenshot window <id> [file]     Grab a single window\n"
        "  screenshot screen [file]          Grab the primary output\n"
        "\n"
        "Interactive:\n"
        "  shell                      Start an interactive REPL (shell mode)\n"
        "  help                       Show this help\n"
        "\n"
        "Backward compatibility: `--tree` and `--cursor` are accepted as aliases for\n"
        "the `tree` and `cursor` commands.\n");
}

} // namespace

// Executes a single one-shot command. Returns a process exit code.
static int runCommand(Session &session, int timeoutMs, bool json,
                      const QString &command, const QStringList &args);

// `top` runs its own event loop.
static int runTop(Session &session, int timeoutMs, int intervalMs);

// `shell` reads commands from stdin.
static int runShell(Session &session, int timeoutMs, bool json);

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("treeland-debug"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QString url = QStringLiteral("local:org.deepin.dde.treeland.debug");
    QString name = QStringLiteral("WindowTree");
    int timeoutMs = 30000;
    bool timeoutOk = true;
    bool json = false;
    bool compatTree = false;
    bool compatCursor = false;
    QStringList rest;

    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromUtf8(argv[i]);
        const auto next = [&]() -> QString {
            if (i + 1 < argc)
                return QString::fromUtf8(argv[++i]);
            return {};
        };
        if (arg == QLatin1String("--url"))
            url = next();
        else if (arg.startsWith(QLatin1String("--url=")))
            url = arg.mid(6);
        else if (arg == QLatin1String("--name"))
            name = next();
        else if (arg.startsWith(QLatin1String("--name=")))
            name = arg.mid(7);
        else if (arg == QLatin1String("--timeout-ms")) {
            bool ok = false;
            timeoutMs = next().toInt(&ok);
            if (!ok)
                timeoutOk = false;
        } else if (arg.startsWith(QLatin1String("--timeout-ms="))) {
            bool ok = false;
            timeoutMs = arg.mid(13).toInt(&ok);
            if (!ok)
                timeoutOk = false;
        }
        else if (arg == QLatin1String("--json"))
            json = true;
        else if (arg == QLatin1String("--tree"))
            compatTree = true;
        else if (arg == QLatin1String("--cursor"))
            compatCursor = true;
        else if (arg == QLatin1String("-h") || arg == QLatin1String("--help")) {
            QTextStream(stdout) << helpText();
            return EXIT_SUCCESS;
        } else if (arg == QLatin1String("-v") || arg == QLatin1String("--version")) {
            QTextStream(stdout) << QCoreApplication::applicationVersion() << Qt::endl;
            return EXIT_SUCCESS;
        } else {
            rest.append(arg);
        }
    }

    if (!timeoutOk || timeoutMs < 0)
        return fail("--timeout-ms must be a non-negative integer");

    registerNamedMetatypes();

    QString command;
    QStringList commandArgs;
    if (compatTree)
        command = QStringLiteral("tree");
    else if (compatCursor)
        command = QStringLiteral("cursor");
    else if (!rest.isEmpty()) {
        command = rest.takeFirst();
        commandArgs = rest;
    } else {
        command = QStringLiteral("tree");
    }

    if (command == QLatin1String("help")) {
        QTextStream(stdout) << helpText();
        return EXIT_SUCCESS;
    }

    Session session;
    if (!connectSession(session, url, name, timeoutMs))
        return fail(QStringLiteral("failed to connect to remote object node: %1").arg(url));

    if (command == QLatin1String("shell"))
        return runShell(session, timeoutMs, json);
    if (command == QLatin1String("top")) {
        int intervalMs = 1000;
        if (!commandArgs.isEmpty())
            intervalMs = commandArgs.first().toInt();
        if (intervalMs <= 0)
            intervalMs = 1000;
        return runTop(session, timeoutMs, intervalMs);
    }

    return runCommand(session, timeoutMs, json, command, commandArgs);
}

static int runCommand(Session &session, int timeoutMs, bool json,
                      const QString &command, const QStringList &args)
{
    auto *replica = session.replica;

    if (command == QLatin1String("tree")) {
        TreelandInfo info;
        if (!waitSlot(replica->getTreelandInfo(), timeoutMs, &info))
            return fail("getTreelandInfo() failed");
        printJson(QJsonDocument(treelandInfoToJson(info)));
        return EXIT_SUCCESS;
    }

    if (command == QLatin1String("cursor")) {
        QPointF pos = replica->cursorPosition();
        printJson(QJsonDocument(pointToJson(pos)));
        return EXIT_SUCCESS;
    }

    if (command == QLatin1String("windows")) {
        QList<WindowInfo> windows;
        if (!waitSlot(replica->getWindows(), timeoutMs, &windows))
            return fail("getWindows() failed");
        if (json)
            printJson(QJsonDocument(windowsToJson(windows)));
        else
            printWindowsTable(windows);
        return EXIT_SUCCESS;
    }

    if (command == QLatin1String("clients")) {
        QList<ClientInfo> clients;
        if (!waitSlot(replica->getClients(), timeoutMs, &clients))
            return fail("getClients() failed");
        if (json)
            printJson(QJsonDocument(clientsToJson(clients)));
        else
            printClientsTable(clients);
        return EXIT_SUCCESS;
    }

    // ---- window control ----
    if (command == QLatin1String("activate") || command == QLatin1String("close")
        || command == QLatin1String("minimize") || command == QLatin1String("maximize")
        || command == QLatin1String("fullscreen") || command == QLatin1String("move")
        || command == QLatin1String("resize") || command == QLatin1String("workspace")) {
        if (args.isEmpty())
            return fail(QStringLiteral("%1: missing window target (id or appId)").arg(command));
        bool ok = false;
        const qint64 id = resolveTarget(session, timeoutMs, args.first(), &ok);
        if (!ok)
            return fail(QStringLiteral("no window matches '%1'").arg(args.first()));

        bool result = false;
        if (command == QLatin1String("activate")) {
            if (!waitSlot(replica->activateWindow(id), timeoutMs, &result))
                return fail("activateWindow() failed");
        } else if (command == QLatin1String("close")) {
            if (!waitSlot(replica->closeWindow(id), timeoutMs, &result))
                return fail("closeWindow() failed");
        } else if (command == QLatin1String("minimize")) {
            if (!waitSlot(replica->minimizeWindow(id), timeoutMs, &result))
                return fail("minimizeWindow() failed");
        } else if (command == QLatin1String("maximize")) {
            if (!waitSlot(replica->toggleMaximized(id), timeoutMs, &result))
                return fail("toggleMaximized() failed");
        } else if (command == QLatin1String("fullscreen")) {
            if (!waitSlot(replica->toggleFullscreen(id), timeoutMs, &result))
                return fail("toggleFullscreen() failed");
        } else if (command == QLatin1String("move")) {
            if (args.size() < 3)
                return fail("move: usage: move <id> <x> <y>");
            const int x = args[1].toInt();
            const int y = args[2].toInt();
            if (!waitSlot(replica->moveWindow(id, x, y), timeoutMs, &result))
                return fail("moveWindow() failed");
        } else if (command == QLatin1String("resize")) {
            if (args.size() < 3)
                return fail("resize: usage: resize <id> <w> <h>");
            const int w = args[1].toInt();
            const int h = args[2].toInt();
            if (!waitSlot(replica->resizeWindow(id, w, h), timeoutMs, &result))
                return fail("resizeWindow() failed");
        } else { // workspace
            if (args.size() < 2)
                return fail("workspace: usage: workspace <id> <workspace-id>");
            const int ws = args[1].toInt();
            if (!waitSlot(replica->setWindowWorkspace(id, ws), timeoutMs, &result))
                return fail("setWindowWorkspace() failed");
        }
        QTextStream(stdout) << (result ? "ok" : "failed") << Qt::endl;
        return result ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // ---- cursor move ----
    if (command == QLatin1String("move-cursor")) {
        if (args.size() < 2)
            return fail("move-cursor: usage: move-cursor <x> <y>");
        bool result = false;
        if (!waitSlot(replica->moveCursor(QPointF(args[0].toDouble(), args[1].toDouble())),
                      timeoutMs, &result))
            return fail("moveCursor() failed");
        QTextStream(stdout) << (result ? "ok" : "failed") << Qt::endl;
        return result ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // ---- event injection ----
    if (command == QLatin1String("event")) {
        if (args.isEmpty())
            return fail("event: usage: event motion|button|key ...");
        const QString sub = args[0];
        bool result = false;
        if (sub == QLatin1String("motion")) {
            if (args.size() < 3)
                return fail("event motion: usage: event motion <x> <y>");
            if (!waitSlot(replica->moveCursor(QPointF(args[1].toDouble(), args[2].toDouble())),
                          timeoutMs, &result))
                return fail("moveCursor() failed");
        } else if (sub == QLatin1String("button")) {
            if (args.size() < 2)
                return fail("event button: usage: event button <left|right|middle|code> [press|release|click]");
            bool codeOk = false;
            const int code = buttonCode(args[1], &codeOk);
            if (!codeOk)
                return fail(QStringLiteral("unknown button '%1'").arg(args[1]));
            const QString act = args.size() > 2 ? args[2] : QStringLiteral("click");
            if (act == QLatin1String("press")) {
                if (!waitSlot(replica->sendPointerButton(code, true), timeoutMs, &result))
                    return fail("sendPointerButton() failed");
            } else if (act == QLatin1String("release")) {
                if (!waitSlot(replica->sendPointerButton(code, false), timeoutMs, &result))
                    return fail("sendPointerButton() failed");
            } else { // click
                bool r1 = false, r2 = false;
                if (!waitSlot(replica->sendPointerButton(code, true), timeoutMs, &r1))
                    return fail("sendPointerButton() failed");
                if (!waitSlot(replica->sendPointerButton(code, false), timeoutMs, &r2))
                    return fail("sendPointerButton() failed");
                result = r1 && r2;
            }
        } else if (sub == QLatin1String("key")) {
            if (args.size() < 2)
                return fail("event key: usage: event key <name|code> [press|release|tap]");
            bool keyOk = false;
            const int code = keyCode(args[1], &keyOk);
            if (!keyOk)
                return fail(QStringLiteral("unknown key '%1'").arg(args[1]));
            const QString act = args.size() > 2 ? args[2] : QStringLiteral("tap");
            if (act == QLatin1String("press")) {
                if (!waitSlot(replica->sendKey(code, true), timeoutMs, &result))
                    return fail("sendKey() failed");
            } else if (act == QLatin1String("release")) {
                if (!waitSlot(replica->sendKey(code, false), timeoutMs, &result))
                    return fail("sendKey() failed");
            } else { // tap
                bool r1 = false, r2 = false;
                if (!waitSlot(replica->sendKey(code, true), timeoutMs, &r1))
                    return fail("sendKey() failed");
                if (!waitSlot(replica->sendKey(code, false), timeoutMs, &r2))
                    return fail("sendKey() failed");
                result = r1 && r2;
            }
        } else {
            return fail(QStringLiteral("event: unknown subcommand '%1'").arg(sub));
        }
        QTextStream(stdout) << (result ? "ok" : "failed") << Qt::endl;
        return result ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // ---- image capture ----
    if (command == QLatin1String("screenshot")) {
        if (args.isEmpty())
            return fail("screenshot: usage: screenshot output|window|screen ...");
        const QString sub = args[0];
        QString path;
        if (sub == QLatin1String("output")) {
            // screenshot output [name] [file]
            QString outputName;
            if (args.size() >= 2 && !args[1].isEmpty())
                outputName = args[1];
            if (args.size() >= 3)
                path = args[2];
            QString result;
            if (!waitSlot(replica->captureOutput(outputName, path), timeoutMs, &result))
                return fail("captureOutput() failed");
            if (result.isEmpty())
                return fail("captureOutput: no image produced (output not found or grab failed)");
            QTextStream(stdout) << result << Qt::endl;
            return EXIT_SUCCESS;
        }
        if (sub == QLatin1String("window")) {
            if (args.size() < 2)
                return fail("screenshot window: usage: screenshot window <id> [file]");
            bool ok = false;
            const qint64 id = resolveTarget(session, timeoutMs, args[1], &ok);
            if (!ok)
                return fail(QStringLiteral("no window matches '%1'").arg(args[1]));
            if (args.size() >= 3)
                path = args[2];
            QString result;
            if (!waitSlot(replica->captureWindow(id, path), timeoutMs, &result))
                return fail("captureWindow() failed");
            if (result.isEmpty())
                return fail("captureWindow: no image produced (grab failed)");
            QTextStream(stdout) << result << Qt::endl;
            return EXIT_SUCCESS;
        }
        if (sub == QLatin1String("screen")) {
            if (args.size() >= 2)
                path = args[1];
            QString result;
            if (!waitSlot(replica->captureScreen(path), timeoutMs, &result))
                return fail("captureScreen() failed");
            if (result.isEmpty())
                return fail("captureScreen: no image produced (grab failed)");
            QTextStream(stdout) << result << Qt::endl;
            return EXIT_SUCCESS;
        }
        return fail(QStringLiteral("screenshot: unknown target '%1'").arg(sub));
    }

    return fail(QStringLiteral("unknown command '%1' (try --help)").arg(command));
}

static int runTop(Session &session, int timeoutMs, int intervalMs)
{
    QTextStream out(stdout);
    out << "treeland-debug top — refreshing every " << intervalMs << "ms (Ctrl+C to quit)\n";
    out.flush();

    int rc = EXIT_SUCCESS;
    QObject context;
    QTimer timer;
    timer.setInterval(intervalMs);

    auto refresh = [&]() {
        QList<ClientInfo> clients;
        if (!waitSlot(session.replica->getClients(), timeoutMs, &clients)) {
            QTextStream(stderr) << "treeland-debug: getClients() failed\n";
            rc = EXIT_FAILURE;
            QCoreApplication::quit();
            return;
        }
        // Clear the terminal and move the cursor home.
        out << QStringLiteral("\033[2J\033[H");
        out << "treeland-debug top — "
            << QDateTime::currentDateTime().toString(Qt::ISODate)
            << "  (clients: " << clients.size() << ")\n";
        printClientsTable(clients);
        out.flush();
    };

    QObject::connect(&timer, &QTimer::timeout, &context, refresh);
    refresh();
    timer.start();

    QCoreApplication::exec();
    return rc;
}

static int runShell(Session &session, int timeoutMs, bool json)
{
    QTextStream out(stdout);
    out << "treeland-debug shell — type 'help' for commands, 'exit' to quit\n";
    QTextStream in(stdin);
    while (true) {
        out << "treeland> ";
        out.flush();
        const QString line = in.readLine();
        if (line.isNull()) // EOF
            break;
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;
        if (trimmed == QLatin1String("exit") || trimmed == QLatin1String("quit"))
            break;
        if (trimmed == QLatin1String("help")) {
            out << helpText();
            continue;
        }

        QStringList parts = trimmed.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty())
            continue;

        const QString command = parts.takeFirst();
        runCommand(session, timeoutMs, json, command, parts);
    }
    return EXIT_SUCCESS;
}
