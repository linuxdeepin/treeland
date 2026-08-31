// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "debugsession.h"

#include <QEventLoop>
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
    // are probing a later one).  A live local socket responds in
    // milliseconds; give earlier (preferred) candidates a short probe and
    // the last one the remaining timeout, so the total never exceeds
    // timeoutMs.
    constexpr int probeTimeout = 3000;
    int remaining = timeoutMs;

    for (int i = 0; i < urls.size(); ++i) {
        const int t = (i < urls.size() - 1) ? qMin(remaining, probeTimeout) : remaining;
        if (t < 0)
            return false;   // budget exhausted

        QRemoteObjectNode probeNode;
        if (!probeNode.connectToNode(QUrl(urls[i])))
            continue;   // malformed URL

        auto *replica = probeNode.acquire<WindowTreeRemoteReplica>(name);
        if (replica->waitForSource(t)) {
            if (connectedUrl)
                *connectedUrl = urls[i];
            // Probe succeeded.  Reconnect the session's own node and acquire
            // a fresh replica, waiting for it to initialize before returning so
            // callers see a ready replica.  Use the remaining budget after the
            // probe so the total never exceeds timeoutMs.  The server is
            // known-live so this completes quickly.
            remaining -= t;
            session.node.connectToNode(QUrl(urls[i]));
            delete session.replica;
            session.replica = session.node.acquire<WindowTreeRemoteReplica>(name);
            const int sessionTimeout = qMin(remaining, probeTimeout);
            if (sessionTimeout < 0 || !session.replica->waitForSource(sessionTimeout)) {
                // Extremely unlikely — the source was just live.  Treat as
                // failure so we don't return an uninitialized replica.
                delete session.replica;
                session.replica = nullptr;
                return false;
            }
            return true;
        }
        remaining -= t;
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
