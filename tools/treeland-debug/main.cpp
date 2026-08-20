// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QProcessEnvironment>
#include <QRemoteObjectNode>
#include <QSocketNotifier>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

#include <csignal>
#include <unistd.h>

#include "debughelpers.h"

#include "debugsession.h"
#include "debugserver.h"

#include "rep_treeland_windowtree_replica.h"

namespace {

// Write-end of a self-pipe used by the SIGINT handler; the handler writes a
// byte here so the Qt event loop (via QSocketNotifier) can react safely.
int g_sigintWriteFd = -1;

void sigintHandler(int)
{
    if (g_sigintWriteFd >= 0) {
        char c = 0;
        const ssize_t r = ::write(g_sigintWriteFd, &c, 1);
        (void)r;
    }
}

// RAII guard that, while alive, routes SIGINT (Ctrl+C) to
// QCoreApplication::quit() instead of terminating the process. Used in the
// shell so live commands (top/events/watch) can be interrupted back to the
// REPL. Uses the self-pipe trick so the signal handler stays async-signal-safe.
class SigIntInterrupt
{
    SigIntInterrupt(const SigIntInterrupt &) = delete;
    SigIntInterrupt &operator=(const SigIntInterrupt &) = delete;

public:
    SigIntInterrupt() = default;
    ~SigIntInterrupt() { restore(); }

    bool install();
    void restore();

private:
    int m_pipe[2] = {-1, -1};
    QSocketNotifier *m_notifier = nullptr;
    struct sigaction m_oldAction {};
    bool m_installed = false;
    QObject m_context;
};

bool SigIntInterrupt::install()
{
    if (::pipe(m_pipe) != 0)
        return false;
    g_sigintWriteFd = m_pipe[1];

    m_notifier = new QSocketNotifier(m_pipe[0], QSocketNotifier::Read, &m_context);
    QObject::connect(m_notifier, &QSocketNotifier::activated, &m_context, [this]() {
        char buf[16];
        while (::read(m_pipe[0], buf, sizeof(buf)) > 0) { }
        QCoreApplication::quit();
    });

    struct sigaction sa {};
    sa.sa_handler = &sigintHandler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    m_installed = (::sigaction(SIGINT, &sa, &m_oldAction) == 0);
    return m_installed;
}

void SigIntInterrupt::restore()
{
    if (m_installed) {
        ::sigaction(SIGINT, &m_oldAction, nullptr);
        m_installed = false;
    }
    g_sigintWriteFd = -1;
    if (m_notifier)
        m_notifier->setEnabled(false);
    if (m_pipe[0] >= 0) {
        ::close(m_pipe[0]);
        m_pipe[0] = -1;
    }
    if (m_pipe[1] >= 0) {
        ::close(m_pipe[1]);
        m_pipe[1] = -1;
    }
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
        out << QStringLiteral("client pid=%1 id=0x%2 %3%4  (windows: %5)\n")
                .arg(client.pid())
                .arg(client.id(), 0, 16)
                .arg(client.appId().isEmpty() ? QString() : QStringLiteral("%1 ").arg(client.appId()))
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

void printTree(const TreelandInfo &info)
{
    QTextStream out(stdout);
    out << "Mode: " << info.currentMode() << "\n";
    const auto layers = info.layers();
    for (int li = 0; li < layers.size(); ++li) {
        const auto &layer = layers[li];
        const QString branch = (li == layers.size() - 1) ? QStringLiteral("└─ ") : QStringLiteral("├─ ");
        out << branch << "Layer: " << layer.name() << " (" << layer.layer() << ")\n";
        // Print standalone windows on this layer (not in any workspace).
        bool hasWorkspaces = !layer.workspaces().isEmpty();
        if (hasWorkspaces) {
            const auto workspaces = layer.workspaces();
            for (int wi = 0; wi < workspaces.size(); ++wi) {
                const auto &ws = workspaces[wi];
                const QString wsBranch = (li == layers.size() - 1) ? QStringLiteral("   ") : QStringLiteral("│  ");
                const QString wsConn = (wi == workspaces.size() - 1) ? QStringLiteral("└─ ") : QStringLiteral("├─ ");
                out << wsBranch << wsConn << "Workspace " << ws.id()
                    << (ws.isActive() ? " (active)" : "") << "\n";
                const auto windows = ws.windows();
                if (windows.isEmpty()) {
                    out << wsBranch << "   └─  (no windows)\n";
                } else {
                    for (int i = 0; i < windows.size(); ++i) {
                        const auto &w = windows[i];
                        const auto g = w.geometry();
                        const QString conn = (i == windows.size() - 1) ? QStringLiteral("└─ ") : QStringLiteral("├─ ");
                        out << wsBranch << "   " << conn
                            << (w.active() ? QStringLiteral("* ") : QStringLiteral("  "))
                            << w.appId() << "  id=" << w.id() << "  "
                            << stateName(w.state()) << "  "
                            << static_cast<int>(g.x()) << "," << static_cast<int>(g.y()) << " "
                            << static_cast<int>(g.width()) << "x" << static_cast<int>(g.height())
                            << "  [" << w.output() << "]"
                            << (w.title().isEmpty() ? "" : "  " + w.title()) << "\n";
                    }
                }
            }
        }
        // Print standalone windows on this layer (no workspace grouping).
        const auto windows = layer.windows();
        for (int i = 0; i < windows.size(); ++i) {
            const auto &w = windows[i];
            const auto g = w.geometry();
            const QString branch2 = (li == layers.size() - 1) ? QStringLiteral("   ") : QStringLiteral("│  ");
            const QString conn = (i == windows.size() - 1) ? QStringLiteral("└─ ") : QStringLiteral("├─ ");
            out << branch2 << conn
                << (w.active() ? QStringLiteral("* ") : QStringLiteral("  "))
                << w.appId() << "  id=" << w.id() << "  "
                << stateName(w.state()) << "  "
                << static_cast<int>(g.x()) << "," << static_cast<int>(g.y()) << " "
                << static_cast<int>(g.width()) << "x" << static_cast<int>(g.height())
                << "  [" << w.output() << "]"
                << (w.title().isEmpty() ? "" : "  " + w.title()) << "\n";
        }
    }
}

// Emit a terminal inline image preview using the kitty graphics or iTerm2 OSC
// 1337 protocol. force=true always emits (best-effort iTerm2 fallback for
// unknown terminals); force=false only emits when the terminal is detected.
bool previewImage(const QByteArray &data, bool force)
{
    if (data.isEmpty())
        return false;

    const QByteArray b64 = data.toBase64();
    QTextStream out(stdout);

    const QByteArray term = qgetenv("TERM");
    const QByteArray prog = qgetenv("TERM_PROGRAM");
    const bool isKitty = term.contains("kitty");
    const bool isItermStyle = prog == "iTerm.app" || prog == "WezTerm"
        || prog == "ghostty" || prog == "konsole" || force;

    if (isKitty) {
        // kitty graphics protocol — chunked transmission.
        // ponytail: 4K chunk size; single-chunk is fine for most screenshots.
        const int chunkSize = 4096;
        for (int offset = 0; offset < b64.size(); offset += chunkSize) {
            const QByteArray chunk = b64.mid(offset, chunkSize);
            const bool first = (offset == 0);
            const bool last = (offset + chunkSize >= b64.size());
            if (first)
                out << QStringLiteral("\033_Ga=T,f=100,m=%1;").arg(last ? 0 : 1);
            else
                out << QStringLiteral("\033_Gm=%1;").arg(last ? 0 : 1);
            out << QString::fromLatin1(chunk) << QStringLiteral("\033\\");
        }
        out << "\n";
        return true;
    }

    if (isItermStyle) {
        // iTerm2 inline image protocol (OSC 1337).
        out << QStringLiteral("\033]1337;File=inline=1;preserveAspectRatio=1:")
            << QString::fromLatin1(b64) << "\a\n";
        return true;
    }

    return false;
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
        "  --json               Emit machine-readable JSON for `tree`/`cursor`/`windows`/`clients`\n"
        "  --preview            Force inline image preview in terminal (auto-detected by default)\n"
        "  --no-preview         Disable inline image preview\n"
        "\n"
        "Inspection:\n"
        "  tree                       Print the complete window tree (human-readable, use --json for JSON)\n"
        "  cursor                     Print the cursor position (human-readable, use --json for JSON)\n"
        "  windows                    List all toplevel windows (use --json for JSON)\n"
        "  clients                    List connected Wayland clients and their windows\n"
        "  top [interval-ms]          Live, top-like refreshing view (default 1000ms)\n"
        "  events [interval-ms]      Real-time input event stream (default 50ms)\n"
        "  watch <id> [interval-ms]  Monitor a window's changes (default 250ms)\n"
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
        "\n"
"Server:\n"
"  listen [--port <port>] [--host <addr>]   Start HTTP/WebSocket server (default 0.0.0.0:8080)\n"
"                             All CLI commands available via HTTP and WebSocket.\n"
"                             Screenshots return raw bytes instead of saving to disk.\n"
"\n"
        "Interactive:\n"
        "  shell                      Start an interactive REPL (shell mode)\n"
        "                             All commands above (including top/events/watch)\n"
        "                             work inside the REPL; Ctrl+C interrupts a live\n"
        "                             view and returns to the prompt.\n"
        "  help                       Show this help\n"
        "\n"
        "Backward compatibility: `--tree` and `--cursor` are accepted as aliases for\n"
        "the `tree` and `cursor` commands.\n");
}

} // namespace

// Executes a single one-shot command. Returns a process exit code.
static int runCommand(Session &session, int timeoutMs, bool json, int previewOpt,
                      const QString &command, const QStringList &args);

// `top` runs its own event loop.
static int runTop(Session &session, int timeoutMs, int intervalMs);

// `events` streams the compositor's real-time event stream.
static int runEvents(Session &session, int timeoutMs, int intervalMs);

// `watch` monitors a single window's changes.
static int runWatch(Session &session, int timeoutMs, qint64 id, int intervalMs);
// `shell` reads commands from stdin.
static int runShell(Session &session, int timeoutMs, bool json, int previewOpt);
// Dispatches the live/refreshing commands (top, events, watch). Returns true
// if @p command was recognized and run (in which case *rc holds the exit
// code); returns false otherwise.
static bool dispatchLiveCommand(Session &session, int timeoutMs,
                                const QString &command, const QStringList &commandArgs,
                                int *rc);

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
    bool previewOpt = 0; // 0=auto-detect, 1=force on, 2=force off
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
        else if (arg == QLatin1String("--preview"))
            previewOpt = 1;
        else if (arg == QLatin1String("--no-preview"))
            previewOpt = 2;
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

    if (command == QLatin1String("listen")) {
        const ParseResult parsed = parseCommand(command, commandArgs);
        if (!parsed.ok)
            return fail(parsed.error);
        DebugServer server(url, name, timeoutMs);
        if (!server.listen(parsed.host, parsed.port))
            return fail(QStringLiteral("listen: failed to bind to %1:%2").arg(parsed.host).arg(parsed.port));
        QTextStream(stdout) << "treeland-debug listening on " << parsed.host << ":" << parsed.port << "\n";
        return QCoreApplication::exec();
    }

    Session session;
    if (!connectSession(session, url, name, timeoutMs))
        return fail(QStringLiteral("failed to connect to remote object node: %1").arg(url));

    if (command == QLatin1String("shell"))
        return runShell(session, timeoutMs, json, previewOpt);
    {
        int liveRc = 0;
        if (dispatchLiveCommand(session, timeoutMs, command, commandArgs, &liveRc))
            return liveRc;
    }

    return runCommand(session, timeoutMs, json, previewOpt, command, commandArgs);
}

static int runCommand(Session &session, int timeoutMs, bool json, int previewOpt,
                      const QString &command, const QStringList &args)
{
    const ParseResult parsed = parseCommand(command, args);
    if (!parsed.ok)
        return fail(parsed.error);

    auto *replica = session.replica;

    switch (parsed.command) {
    case DebugCommand::Tree: {
        TreelandInfo info;
        if (!waitSlot(replica->getTreelandInfo(), timeoutMs, &info))
            return fail("getTreelandInfo() failed");
        if (json)
            printJson(QJsonDocument(treelandInfoToJson(info)));
        else
            printTree(info);
        return EXIT_SUCCESS;
    }
    case DebugCommand::Cursor: {
        QPointF pos = replica->cursorPosition();
        if (json)
            printJson(QJsonDocument(pointToJson(pos)));
        else
            QTextStream(stdout) << "x=" << pos.x() << " y=" << pos.y() << Qt::endl;
        return EXIT_SUCCESS;
    }
    case DebugCommand::Windows: {
        QList<WindowInfo> windows;
        if (!waitSlot(replica->getWindows(), timeoutMs, &windows))
            return fail("getWindows() failed");
        if (json)
            printJson(QJsonDocument(windowsToJson(windows)));
        else
            printWindowsTable(windows);
        return EXIT_SUCCESS;
    }
    case DebugCommand::Clients: {
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
    case DebugCommand::Activate:
    case DebugCommand::Close:
    case DebugCommand::Minimize:
    case DebugCommand::Maximize:
    case DebugCommand::Fullscreen:
    case DebugCommand::Move:
    case DebugCommand::Resize:
    case DebugCommand::Workspace: {
        bool ok = false;
        const qint64 id = resolveTarget(session, timeoutMs, parsed.target, &ok);
        if (!ok)
            return fail(QStringLiteral("no window matches '%1'").arg(parsed.target));

        bool result = false;
        switch (parsed.command) {
        case DebugCommand::Activate:
            if (!waitSlot(replica->activateWindow(id), timeoutMs, &result))
                return fail("activateWindow() failed");
            break;
        case DebugCommand::Close:
            if (!waitSlot(replica->closeWindow(id), timeoutMs, &result))
                return fail("closeWindow() failed");
            break;
        case DebugCommand::Minimize:
            if (!waitSlot(replica->minimizeWindow(id), timeoutMs, &result))
                return fail("minimizeWindow() failed");
            break;
        case DebugCommand::Maximize:
            if (!waitSlot(replica->toggleMaximized(id), timeoutMs, &result))
                return fail("toggleMaximized() failed");
            break;
        case DebugCommand::Fullscreen:
            if (!waitSlot(replica->toggleFullscreen(id), timeoutMs, &result))
                return fail("toggleFullscreen() failed");
            break;
        case DebugCommand::Move:
            if (!waitSlot(replica->moveWindow(id, parsed.x, parsed.y), timeoutMs, &result))
                return fail("moveWindow() failed");
            break;
        case DebugCommand::Resize:
            if (!waitSlot(replica->resizeWindow(id, parsed.width, parsed.height), timeoutMs, &result))
                return fail("resizeWindow() failed");
            break;
        case DebugCommand::Workspace:
            if (!waitSlot(replica->setWindowWorkspace(id, parsed.workspaceId), timeoutMs, &result))
                return fail("setWindowWorkspace() failed");
            break;
        default:
            break;
        }
        QTextStream(stdout) << (result ? "ok" : "failed") << Qt::endl;
        return result ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    // ---- cursor move ----
    case DebugCommand::MoveCursor: {
        bool result = false;
        if (!waitSlot(replica->moveCursor(QPointF(parsed.dx, parsed.dy)), timeoutMs, &result))
            return fail("moveCursor() failed");
        QTextStream(stdout) << (result ? "ok" : "failed") << Qt::endl;
        return result ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    // ---- event injection ----
    case DebugCommand::EventMotion: {
        bool result = false;
        if (!waitSlot(replica->moveCursor(QPointF(parsed.dx, parsed.dy)), timeoutMs, &result))
            return fail("moveCursor() failed");
        QTextStream(stdout) << (result ? "ok" : "failed") << Qt::endl;
        return result ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    case DebugCommand::EventButton: {
        bool result = false;
        const int code = parsed.code;
        const QString act = parsed.action;
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
        QTextStream(stdout) << (result ? "ok" : "failed") << Qt::endl;
        return result ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    case DebugCommand::EventKey: {
        bool result = false;
        const int code = parsed.code;
        const QString act = parsed.action;
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
        QTextStream(stdout) << (result ? "ok" : "failed") << Qt::endl;
        return result ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    // ---- image capture ----
    case DebugCommand::ScreenshotOutput: {
        QByteArray data;
        if (!waitSlot(replica->captureOutput(parsed.outputName), timeoutMs, &data))
            return fail("captureOutput() failed");
        if (data.isEmpty())
            return fail("captureOutput: no image produced (output not found or grab failed)");
        const QString saved = saveCapture(data, parsed.filePath);
        if (saved.isEmpty())
            return fail("captureOutput: failed to save image");
        QTextStream(stdout) << saved << Qt::endl;
        if (previewOpt != 2)
            previewImage(data, previewOpt == 1);
        return EXIT_SUCCESS;
    }
    case DebugCommand::ScreenshotWindow: {
        bool ok = false;
        const qint64 id = resolveTarget(session, timeoutMs, parsed.target, &ok);
        if (!ok)
            return fail(QStringLiteral("no window matches '%1'").arg(parsed.target));
        QByteArray data;
        if (!waitSlot(replica->captureWindow(id), timeoutMs, &data))
            return fail("captureWindow() failed");
        if (data.isEmpty())
            return fail("captureWindow: no image produced (grab failed)");
        const QString saved = saveCapture(data, parsed.filePath);
        if (saved.isEmpty())
            return fail("captureWindow: failed to save image");
        QTextStream(stdout) << saved << Qt::endl;
        if (previewOpt != 2)
            previewImage(data, previewOpt == 1);
        return EXIT_SUCCESS;
    }
    default:
        return fail(QStringLiteral("unknown command '%1' (try --help)").arg(command));
    }
}

static int runTop(Session &session, int timeoutMs, int intervalMs)
{
    QTextStream out(stdout);
    out << "treeland-debug top — refreshing every " << intervalMs << "ms (Ctrl+C to quit)\n";
    out.flush();
    // Keep previous frames to compute per-interval deltas for sorting.
    QHash<qint64, qint64> prevFrames;

    int rc = EXIT_SUCCESS;
    QObject context;
    QTimer timer;
    timer.setInterval(intervalMs);

    auto refresh = [&]() {
        // Fetch current status.
        QList<ClientInfo> clients;
        if (!waitSlot(session.replica->getClients(), timeoutMs, &clients)) {
            QTextStream(stderr) << "treeland-debug: getClients() failed\n";
            rc = EXIT_FAILURE;
            QCoreApplication::quit();
            return;
        }
        qint64 focusId = 0, cursorId = 0;
        waitSlot(session.replica->focusedWindowId(), timeoutMs, &focusId);
        waitSlot(session.replica->windowUnderCursor(), timeoutMs, &cursorId);

        // Collect all windows with their frame deltas.
        struct WinRow {
            qint64 id; QString appId; QString title; QString output;
            int state; QRectF geo; bool active;
            qint64 frames; QRectF damage;
            int64_t framesDelta;
        };
        QList<WinRow> rows;
        for (const auto &cl : clients) {
            for (const auto &w : cl.windows()) {
                WinRow r;
                r.id = w.id(); r.appId = w.appId(); r.title = w.title();
                r.output = w.output(); r.state = w.state(); r.active = w.active();
                r.geo = w.geometry(); r.frames = w.frames(); r.damage = w.damage();
                const auto prev = prevFrames.value(w.id());
                r.framesDelta = (prev > 0) ? (w.frames() - prev) : 0;
                rows.append(r);
            }
        }
        // Update prevFrames for next cycle.
        for (const auto &r : rows)
            prevFrames[r.id] = r.frames;

        // Sort by framesDelta descending (most active first).
        std::sort(rows.begin(), rows.end(),
                  [](const WinRow &a, const WinRow &b) { return a.framesDelta > b.framesDelta; });

        // Clear and print.
        out << QStringLiteral("\033[2J\033[H");
        out << "treeland-debug top — "
            << QDateTime::currentDateTime().toString(Qt::ISODate)
            << "  (clients: " << clients.size() << "  windows: " << rows.size() << ")\n"
            << "  ID              APP-ID                STATE       FRAMES  GEO          MARKER\n";
        for (const auto &r : rows) {
            QString marker;
            if (r.id == focusId) marker = QStringLiteral("F");
            else if (r.id == cursorId) marker = QStringLiteral("C");
            else marker = QStringLiteral(" ");
            out << QStringLiteral("  %1  %2  %3  %4  %5,%6 %7x%8  %9\n")
                .arg(QString::number(r.id).leftJustified(16))
                .arg(r.appId.leftJustified(22))
                .arg(stateName(r.state).leftJustified(12))
                .arg(r.framesDelta, 5)
                .arg(static_cast<int>(r.geo.x()))
                .arg(static_cast<int>(r.geo.y()))
                .arg(static_cast<int>(r.geo.width()))
                .arg(static_cast<int>(r.geo.height()))
                .arg(marker);
        }
        out.flush();
    };

    QObject::connect(&timer, &QTimer::timeout, &context, refresh);
    refresh();
    timer.start();

    QCoreApplication::exec();
    return rc;
}

static bool dispatchLiveCommand(Session &session, int timeoutMs,
                                const QString &command, const QStringList &commandArgs,
                                int *rc)
{
    const ParseResult parsed = parseCommand(command, commandArgs);

    // Only the live commands (top/events/watch) are handled here; everything
    // else falls through to runCommand().
    if (parsed.command != DebugCommand::Top && parsed.command != DebugCommand::Events
        && parsed.command != DebugCommand::Watch) {
        return false;
    }

    if (!parsed.ok) {
        *rc = fail(parsed.error);
        return true;
    }

    switch (parsed.command) {
    case DebugCommand::Top:
        *rc = runTop(session, timeoutMs, parsed.intervalMs);
        return true;
    case DebugCommand::Events:
        *rc = runEvents(session, timeoutMs, parsed.intervalMs);
        return true;
    case DebugCommand::Watch: {
        bool ok = false;
        const qint64 id = resolveTarget(session, timeoutMs, parsed.target, &ok);
        if (!ok) {
            *rc = fail(QStringLiteral("no window matches '%1'").arg(parsed.target));
            return true;
        }
        *rc = runWatch(session, timeoutMs, id, parsed.intervalMs);
        return true;
    }
    default:
        return false;
    }
}

static int runShell(Session &session, int timeoutMs, bool json, int previewOpt)
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
        // Live commands (top/events/watch) run their own event loop. Install a
        // SIGINT guard so Ctrl+C interrupts the live view and returns to the
        // REPL instead of killing the process. For non-live commands the guard
        // is installed only briefly (dispatchLiveCommand returns immediately).
        {
            SigIntInterrupt guard;
            guard.install();
            int liveRc = 0;
            if (dispatchLiveCommand(session, timeoutMs, command, parts, &liveRc)) {
                out << "\n"; // newline after the interrupted live view
                continue;
            }
        }
        runCommand(session, timeoutMs, json, previewOpt, command, parts);
    }
    return EXIT_SUCCESS;
}

static int runEvents(Session &session, int timeoutMs, int intervalMs)
{
    QTextStream out(stdout);
    out << "treeland-debug events — streaming every " << intervalMs << "ms (Ctrl+C to quit)\n\n";
    out.flush();

    int rc = EXIT_SUCCESS;
    quint64 lastSeq = 0;
    QObject context;
    QTimer timer;
    timer.setInterval(intervalMs);

    auto refresh = [&]() {
        QList<DebugEvent> events;
        if (!waitSlot(session.replica->getEvents(lastSeq), timeoutMs, &events)) {
            QTextStream(stderr) << "treeland-debug: getEvents() failed\n";
            rc = EXIT_FAILURE;
            QCoreApplication::quit();
            return;
        }
        for (const auto &e : events) {
            lastSeq = std::max(lastSeq, e.seq());
            const QString time = QDateTime::fromMSecsSinceEpoch(e.timestampMs()).toString("HH:mm:ss.zzz");
            QString targetName;
            if (e.target() != 0)
                targetName = QStringLiteral(" -> %1").arg(e.target());
            out << time << "  " << e.detail() << targetName << "\n";
        }
        out.flush();
    };

    QObject::connect(&timer, &QTimer::timeout, &context, refresh);
    refresh();
    timer.start();

    QCoreApplication::exec();
    return rc;
}

static int runWatch(Session &session, int timeoutMs, qint64 id, int intervalMs)
{
    QTextStream out(stdout);
    out << "treeland-debug watch " << id << " — every " << intervalMs << "ms (Ctrl+C to quit)\n\n";
    out.flush();

    int rc = EXIT_SUCCESS;
    WindowInfo prev;
    bool havePrev = false;
    qint64 prevFrames = 0;
    QObject context;
    QTimer timer;
    timer.setInterval(intervalMs);

    auto refresh = [&]() {
        QList<WindowInfo> windows;
        if (!waitSlot(session.replica->getWindows(), timeoutMs, &windows)) {
            QTextStream(stderr) << "treeland-debug: getWindows() failed\n";
            rc = EXIT_FAILURE;
            QCoreApplication::quit();
            return;
        }
        WindowInfo cur;
        bool found = false;
        for (const auto &w : windows) {
            if (w.id() == id) { cur = w; found = true; break; }
        }
        if (!found) {
            QTextStream(stderr) << "treeland-debug: window " << id << " no longer exists\n";
            rc = EXIT_FAILURE;
            QCoreApplication::quit();
            return;
        }

        // Print a header on the first sample, then one event-style line per
        // change on subsequent samples.
        const bool firstSample = !havePrev;
        if (firstSample) {
            out << "Window " << id << " " << cur.appId()
                << (cur.title().isEmpty() ? QString() : QStringLiteral(" (%1)").arg(cur.title()))
                << "\n";
            havePrev = true;
        }
        QStringList changes;
        if (!firstSample) {
            if (cur.geometry() != prev.geometry())
                changes << QStringLiteral("geometry %1,%2 %3x%4")
                    .arg(cur.geometry().x()).arg(cur.geometry().y())
                    .arg(cur.geometry().width()).arg(cur.geometry().height());
            if (cur.workspace() != prev.workspace())
                changes << QStringLiteral("workspace %1").arg(cur.workspace());
            if (cur.state() != prev.state())
                changes << QStringLiteral("state %1 -> %2")
                    .arg(stateName(prev.state())).arg(stateName(cur.state()));
            if (cur.active() != prev.active())
                changes << (cur.active() ? QStringLiteral("activated") : QStringLiteral("deactivated"));
        }
        const qint64 frames = cur.frames();
        if (frames != prevFrames)
            changes << QStringLiteral("frames %1 (+%2)").arg(frames).arg(frames - prevFrames);
        if (!cur.damage().isEmpty())
            changes << QStringLiteral("damage %1,%2 %3x%4")
                .arg(cur.damage().x()).arg(cur.damage().y())
                .arg(cur.damage().width()).arg(cur.damage().height());
        prevFrames = frames;

        // Always print one line per refresh (like `top`); changes are prefixed
        // with `*` when they differ from the previous snapshot.
        const QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
        out << time << "  ";
        if (changes.isEmpty())
            out << "(no change)\n";
        else
            out << "  * " << changes.join("; ") << "\n";
        prev = cur;
        out.flush();
    };

    QObject::connect(&timer, &QTimer::timeout, &context, refresh);
    refresh();
    timer.start();

    QCoreApplication::exec();
    return rc;
}
