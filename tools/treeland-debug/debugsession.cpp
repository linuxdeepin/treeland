// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "debugsession.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QTimer>

bool connectSession(Session &session, const QString &url, const QString &name, int timeoutMs)
{
    if (!session.node.connectToNode(QUrl(url)))
        return false;
    session.replica = session.node.acquire<WindowTreeRemoteReplica>(name);
    return session.replica->waitForSource(timeoutMs);
}

bool connectSession(Session &session, const QStringList &urls,
                     const QString &name, int timeoutMs,
                     QString *connectedUrl)
{
    if (urls.isEmpty())
        return false;

    // Try each URL with an independent QRemoteObjectNode so a previously-
    // failed connection cannot cause a false success on a later candidate
    // (the replica watches *all* connected nodes — a shared node would
    // let a late-becoming-reachable earlier URL succeed while we think we
    // are probing a later one).
    //
    // Share the caller's timeout between the candidates still to be tried, but
    // never probe with less than probeTimeout: a live local socket answers in
    // milliseconds, so a slice only bounds the wait, while a socket that cannot
    // answer costs its whole slice either way (the replica waits for a source that
    // never arrives).  A larger slice also lets a compositor that publishes its
    // source a moment after the client starts still be found.  Charging only the
    // time a probe actually spent keeps the later slices large, and the slice is
    // never zero — handing the leftovers of the earlier dead sockets to the
    // preferred instance made it skip a healthy debug instance for the release one.
    constexpr int probeTimeout = 3000;
    int remaining = timeoutMs;
    QElapsedTimer probeTimer;

    for (int i = 0; i < urls.size(); ++i) {
        const int candidatesLeft = urls.size() - i;
        const int t = qMax(probeTimeout, remaining / candidatesLeft);

        QRemoteObjectNode probeNode;
        if (!probeNode.connectToNode(QUrl(urls.at(i))))
            continue;   // malformed URL

        probeTimer.restart();
        auto *replica = probeNode.acquire<WindowTreeRemoteReplica>(name);
        const bool answered = replica->waitForSource(t);
        remaining -= qMin(t, int(probeTimer.elapsed()));
        if (!answered)
            continue;

        // Probe succeeded.  Reconnect the session's own node and acquire a fresh
        // replica, waiting for it to initialize before returning so callers see a
        // ready replica.  The source was just proven live, so this is a formality
        // with the same probe-sized wait.
        session.node.connectToNode(QUrl(urls.at(i)));
        delete session.replica;
        session.replica = session.node.acquire<WindowTreeRemoteReplica>(name);
        if (session.replica->waitForSource(probeTimeout)) {
            if (connectedUrl)
                *connectedUrl = urls.at(i);
            return true;
        }
        // The source vanished in between.  Stop instead of trying the next
        // candidate: QtRO cannot drop the connection just made, and a replica
        // watches every node the session is connected to, so a source that comes
        // back there could satisfy a later wait and send the commands to another
        // compositor.  Failing loudly is the safer answer for a race this narrow.
        delete session.replica;
        session.replica = nullptr;
        return false;
    }
    return false;
}

bool waitCaptureResult(WindowTreeRemoteReplica *replica, int timeoutMs, QByteArray *out)
{
    QEventLoop loop;
    bool gotResult = false;
    QByteArray result;

    auto conn = QObject::connect(replica, &WindowTreeRemoteReplica::captureResult,
        [&loop, &result, &gotResult](const QByteArray &data) {
            result = data;
            gotResult = true;
            loop.quit();
        });

    // Overall timeout.
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);

    // Periodic interrupt check for live mode (Ctrl+C sets g_debugInterrupted).
    QTimer interruptTimer;
    interruptTimer.setSingleShot(false);
    QObject::connect(&interruptTimer, &QTimer::timeout, &loop, [&loop]() {
        if (g_debugInterrupted)
            loop.quit();
    });
    interruptTimer.start(100);

    loop.exec();
    QObject::disconnect(conn);

    if (!gotResult)
        return false;
    if (out)
        *out = result;
    return true;
}

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
    qRegisterMetaType<IdleInhibitor>("IdleInhibitor");
    qRegisterMetaType<QList<IdleInhibitor>>("QList<IdleInhibitor>");
    qRegisterMetaType<IdleInfo>("IdleInfo");
}

QJsonObject windowToJson(const WindowInfo &window)
{
    return {
        {"id", window.id()},
        {"appId", window.appId()},
        {"title", window.title()},
        {"output", window.output()},
        {"outputs", QJsonArray::fromStringList(window.outputs())},
        {"framePacingOutput", window.framePacingOutput()},
        {"container", window.container()},
        {"workspace", window.workspace()},
        {"layer", window.layer()},
        {"z", window.z()},
        {"type", window.type()},
        {"state", window.state()},
        {"visible", window.visible()},
        {"active", window.active()},
        {"minimized", window.minimized()},
        {"geometry", rectToJson(window.geometry())},
        {"titlebarGeometry", rectToJson(window.titlebarGeometry())},
        {"boundingRect", rectToJson(window.boundingRect())},
        {"iconGeometry", rectToJson(window.iconGeometry())},
        {"position", pointToJson(window.position())},
        {"frames", window.frames()},
        {"damage", rectToJson(window.damage())},
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
        {"appId", client.appId()},
        {"pid", client.pid()},
        {"executable", client.executable()},
        {"command", client.command()},
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

QJsonObject shortcutToJson(const ShortcutInfo &shortcut)
{
    return {
        {"name", shortcut.name()},
        {"type", shortcut.type()},
        {"key", shortcut.key()},
        {"flags", static_cast<qint64>(shortcut.flags())},
        {"action", shortcut.action()},
        {"fingers", static_cast<qint64>(shortcut.fingers())},
        {"direction", shortcut.direction()},
    };
}

QJsonArray shortcutsToJson(const QList<ShortcutInfo> &shortcuts)
{
    QJsonArray result;
    for (const auto &shortcut : shortcuts)
        result.append(shortcutToJson(shortcut));
    return result;
}

QJsonObject debugEventToJson(const DebugEvent &event)
{
    return {
        {"seq", static_cast<qint64>(event.seq())},
        {"type", event.type()},
        {"target", event.target()},
        {"detail", event.detail()},
        {"timestampMs", static_cast<qint64>(event.timestampMs())},
    };
}

QJsonArray debugEventsToJson(const QList<DebugEvent> &events)
{
    QJsonArray result;
    for (const auto &event : events)
        result.append(debugEventToJson(event));
    return result;
}

QJsonObject idleInhibitorToJson(const IdleInhibitor &inhibitor)
{
    return {
        {"surfaceId", inhibitor.surfaceId()},
        {"kind", inhibitor.kind()},
        {"appId", inhibitor.appId()},
        {"title", inhibitor.title()},
        {"pid", inhibitor.pid()},
        {"executable", inhibitor.executable()},
        {"command", inhibitor.command()},
        {"appName", inhibitor.appName()},
        {"reason", inhibitor.reason()},
        {"active", inhibitor.active()},
        {"heldMs", static_cast<qint64>(inhibitor.heldMs())},
    };
}

QJsonObject idleToJson(const IdleInfo &info)
{
    QJsonArray inhibitors;
    for (const auto &inhibitor : info.inhibitors())
        inhibitors.append(idleInhibitorToJson(inhibitor));

    return {
        {"inhibited", info.inhibited()},
        {"screensaverInhibited", info.screensaverInhibited()},
        {"idleMs", static_cast<qint64>(info.idleMs())},
        {"inhibitors", inhibitors},
    };
}
