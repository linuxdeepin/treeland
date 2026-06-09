// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "dummyddmstate.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <utility>

namespace {
DummySessionEntry loadSessionEntry(const QJsonObject &object)
{
    return {
        object.value(QStringLiteral("fileName")).toString(),
        object.value(QStringLiteral("type")).toInt(),
        object.value(QStringLiteral("displayName")).toString(),
        object.value(QStringLiteral("comment")).toString(),
        object.value(QStringLiteral("exec")).toString(),
    };
}

DummyUserSession loadUserSession(const QJsonObject &object)
{
    return {
        object.value(QStringLiteral("user")).toString(),
        object.value(QStringLiteral("sessionId")).toInt(-1),
    };
}
}

bool DummyDdmState::loadFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (document.isNull() || !document.isObject())
        return false;

    DummyDdmState nextState;
    const QJsonObject root = document.object();
    nextState.hostName = root.value(QStringLiteral("hostName")).toString();
    nextState.currentUser = root.value(QStringLiteral("currentUser")).toString();
    nextState.lastUser = root.value(QStringLiteral("lastUser")).toString();
    nextState.lastSession = root.value(QStringLiteral("lastSession")).toString();
    nextState.rememberLastSession = root.value(QStringLiteral("rememberLastSession")).toBool();
    nextState.treelandLocked = root.value(QStringLiteral("treelandLocked")).toBool();

    const QJsonObject power = root.value(QStringLiteral("power")).toObject();
    nextState.canPowerOff = power.value(QStringLiteral("powerOff")).toBool();
    nextState.canReboot = power.value(QStringLiteral("reboot")).toBool();
    nextState.canSuspend = power.value(QStringLiteral("suspend")).toBool();
    nextState.canHibernate = power.value(QStringLiteral("hibernate")).toBool();
    nextState.canHybridSleep = power.value(QStringLiteral("hybridSleep")).toBool();

    const QJsonArray sessionArray = root.value(QStringLiteral("sessions")).toArray();
    nextState.sessions.reserve(sessionArray.size());
    for (const QJsonValue &value : sessionArray)
        nextState.sessions.append(loadSessionEntry(value.toObject()));

    const QJsonArray userArray = root.value(QStringLiteral("users")).toArray();
    nextState.users.reserve(userArray.size());
    for (const QJsonValue &value : userArray)
        nextState.users.append(value.toString());

    const QJsonArray activeSessionArray = root.value(QStringLiteral("activeSessions")).toArray();
    nextState.activeSessions.reserve(activeSessionArray.size());
    for (const QJsonValue &value : activeSessionArray)
        nextState.activeSessions.append(loadUserSession(value.toObject()));

    *this = std::move(nextState);
    return true;
}
