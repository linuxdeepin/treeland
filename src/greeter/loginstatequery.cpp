// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "loginstatequery.h"

#include "common/treelandlogging.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

LoginStateQuery::LoginStateQuery(QObject *parent)
    : QObject(parent)
    , m_timeout(this)
{
    m_timeout.setSingleShot(true);
    m_timeout.setTimerType(Qt::PreciseTimer);
    m_timeout.setInterval(3000);
    connect(&m_timeout, &QTimer::timeout, this, &LoginStateQuery::onTimeout);
}

void LoginStateQuery::start()
{
    if (m_started) {
        return;
    }

    m_started = true;
    m_timeout.start();

    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DisplayManager"),
                                                QStringLiteral("/org/freedesktop/DisplayManager"),
                                                QStringLiteral("org.freedesktop.DBus.Properties"),
                                                QStringLiteral("Get"));
    message << QStringLiteral("org.freedesktop.DisplayManager") << QStringLiteral("Sessions");
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::systemBus().asyncCall(message, m_timeout.interval()), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &LoginStateQuery::onSessionsQueried);
}

void LoginStateQuery::onSessionsQueried(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();
    if (m_finished) {
        return;
    }

    const QDBusPendingReply<QDBusVariant> reply = *watcher;
    if (reply.isError()) {
        qCWarning(lcTlHooks) << "Cannot query DDM sessions; running prestart scripts:"
                            << reply.error().message();
        finish(false);
        return;
    }

    const auto sessions = qdbus_cast<QList<QDBusObjectPath>>(reply.value().variant());
    m_pendingSessionQueries = sessions.size();
    if (sessions.isEmpty()) {
        finish(false);
        return;
    }

    // Query session users in parallel under the same overall deadline.
    for (const auto &path : sessions) {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DisplayManager"),
                                                    path.path(),
                                                    QStringLiteral("org.freedesktop.DBus.Properties"),
                                                    QStringLiteral("Get"));
        message << QStringLiteral("org.freedesktop.DisplayManager.Session") << QStringLiteral("UserName");
        auto *userWatcher = new QDBusPendingCallWatcher(
            QDBusConnection::systemBus().asyncCall(message, m_timeout.interval()), this);
        connect(userWatcher, &QDBusPendingCallWatcher::finished,
                this, &LoginStateQuery::onSessionUserQueried);
    }
}

void LoginStateQuery::onSessionUserQueried(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();
    if (m_finished) {
        return;
    }

    const QDBusPendingReply<QDBusVariant> reply = *watcher;
    const auto userName = reply.isError() ? QString() : reply.value().variant().toString();
    if (reply.isError() || userName.isEmpty()) {
        qCWarning(lcTlHooks) << "Cannot query DDM session user:" << reply.error().message();
    } else if (userName != QLatin1String("dde")) {
        // DDM registers its greeter as dde; other users have logged-in sessions.
        qCInfo(lcTlHooks) << "DDM user session already exists; skipping prestart scripts:"
                         << userName;
        finish(true);
        return;
    }

    if (--m_pendingSessionQueries == 0) {
        finish(false);
    }
}

void LoginStateQuery::onTimeout()
{
    if (m_finished) {
        return;
    }

    qCWarning(lcTlHooks) << "DDM login query timed out; running prestart scripts";
    finish(false);
}

void LoginStateQuery::finish(bool hasLoggedInUser)
{
    if (m_finished) {
        return;
    }

    m_finished = true;
    m_timeout.stop();
    Q_EMIT finished(hasLoggedInUser);
}
