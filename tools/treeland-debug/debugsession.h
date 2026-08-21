// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef TREELAND_DEBUG_SESSION_H
#define TREELAND_DEBUG_SESSION_H

#include "debughelpers.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QRemoteObjectNode>
#include <QString>

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

// Waits for a typed replica slot call and stores its return value in @p out.
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

#endif // TREELAND_DEBUG_SESSION_H
