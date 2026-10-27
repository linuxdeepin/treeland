// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QTimer>

class QDBusPendingCallWatcher;

class LoginStateQuery : public QObject
{
    Q_OBJECT
public:
    explicit LoginStateQuery(QObject *parent = nullptr);

    void start();

Q_SIGNALS:
    // False also covers an unsuccessful query or an expired deadline.
    void finished(bool hasLoggedInUser);

private:
    void onSessionsQueried(QDBusPendingCallWatcher *watcher);
    void onSessionUserQueried(QDBusPendingCallWatcher *watcher);
    void onTimeout();
    void finish(bool hasLoggedInUser);

    QTimer m_timeout;
    qsizetype m_pendingSessionQueries{ 0 };
    bool m_started{ false };
    bool m_finished{ false };
};
