// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct DummySessionEntry {
    QString fileName;
    int type = 0;
    QString displayName;
    QString comment;
    QString exec;
};

struct DummyUserSession {
    QString user;
    int sessionId = -1;
};

class DummyDdmState
{
public:
    bool loadFromFile(const QString &path);

    QString hostName;
    QString currentUser;
    QString lastUser;
    QString lastSession;
    bool rememberLastSession = false;
    bool treelandLocked = false;
    bool canPowerOff = false;
    bool canReboot = false;
    bool canSuspend = false;
    bool canHibernate = false;
    bool canHybridSleep = false;
    QList<DummySessionEntry> sessions;
    QStringList users;
    QList<DummyUserSession> activeSessions;
};
