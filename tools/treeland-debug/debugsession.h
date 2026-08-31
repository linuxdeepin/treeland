// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "debughelpers.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRemoteObjectNode>
#include <QString>
#include <csignal>

#include "rep_treeland_windowtree_replica.h"

// A connection to the running Treeland debug Remote Object.
struct Session
{
    QRemoteObjectNode node;
    WindowTreeRemoteReplica *replica = nullptr;
};

// Connects @p session to the Treeland debug remote object at @p url with the
// given @p name. Returns true on success.
bool connectSession(Session &session, const QString &url, const QString &name, int timeoutMs);

// Tries each URL in @p urls in order, connecting to the first whose source is
// reachable. Earlier (higher-preference) URLs get a short probe timeout; the
// last URL gets the full remaining timeout. If @p connectedUrl is non-null it
// receives the URL that succeeded. An empty list returns false.
bool connectSession(Session &session, const QStringList &urls,
                     const QString &name, int timeoutMs,
                     QString *connectedUrl = nullptr);

// Set to 1 by the SIGINT handler during interactive ("live") commands so the
// blocking waitSlot() below can break out promptly instead of being stuck in
// a single waitForFinished() until the RPC timeout. It is async-signal-safe
// (a plain sig_atomic_t store) and stays 0 outside live commands, so one-shot
// commands keep their original behaviour.
extern volatile sig_atomic_t g_debugInterrupted;

// Waits for a typed replica slot call and stores its return value in @p out.
// The wait is polled in small chunks so a Ctrl+C during a live command (which
// sets g_debugInterrupted) breaks the wait within ~100ms instead of blocking
// until the full RPC timeout. For one-shot commands g_debugInterrupted is
// never set, so the effective behaviour is unchanged.
template <typename T>
bool waitSlot(QRemoteObjectPendingReply<T> call, int timeoutMs, T *out)
{
    const int chunk = 100;
    int waited = 0;
    while (!call.waitForFinished(qBound(1, timeoutMs - waited, chunk))) {
        if (g_debugInterrupted)
            return false;
        waited += chunk;
        if (waited >= timeoutMs)
            return false;
    }
    if (call.error() != QRemoteObjectPendingCall::NoError)
        return false;
    if (out)
        *out = call.returnValue();
    return true;
}

// Waits for the async captureResult signal after calling captureOutput() or
// captureWindow(). Returns true if the signal was received within @p timeoutMs
// (or interrupted by Ctrl+C in live mode), storing the payload in @p out.
bool waitCaptureResult(WindowTreeRemoteReplica *replica, int timeoutMs, QByteArray *out);

// Resolves a window target: a numeric id is used as-is, any other token is
// matched against the first window whose appId equals it. Sets *ok=false on
// failure.
qint64 resolveTarget(Session &session, int timeoutMs, const QString &token, bool *ok);

// Registers the metatypes needed for serialising replica structs across
// the Qt RemoteObjects wire.
void registerNamedMetatypes();

// JSON helpers for the replica POD structs.
QJsonObject windowToJson(const WindowInfo &window);
QJsonArray windowsToJson(const QList<WindowInfo> &windows);
QJsonObject workspaceToJson(const WorkspaceInfo &workspace);
QJsonArray workspacesToJson(const QList<WorkspaceInfo> &workspaces);
QJsonObject layerToJson(const LayerInfo &layer);
QJsonArray layersToJson(const QList<LayerInfo> &layers);
QJsonObject treelandInfoToJson(const TreelandInfo &info);
QJsonObject clientToJson(const ClientInfo &client);
QJsonArray clientsToJson(const QList<ClientInfo> &clients);
QJsonObject debugEventToJson(const DebugEvent &event);
QJsonArray debugEventsToJson(const QList<DebugEvent> &events);

