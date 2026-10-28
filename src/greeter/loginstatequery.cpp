// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "loginstatequery.h"

#include "common/treelandlogging.h"
#include "utils/loginddbustypes.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>

LoginStateQuery::LoginStateQuery(QObject *parent)
    : QObject(parent)
{
}

void LoginStateQuery::start()
{
    if (m_started) {
        return;
    }

    m_started = true;

    auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"),
                                                QStringLiteral("/org/freedesktop/login1"),
                                                QStringLiteral("org.freedesktop.login1.Manager"),
                                                QStringLiteral("ListSessions"));
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, &LoginStateQuery::onSessionsQueried);
}

void LoginStateQuery::onSessionsQueried(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();
    if (m_finished) {
        return;
    }

    const QDBusPendingReply<SessionInfoList> reply = *watcher;
    if (reply.isError()) {
        qCWarning(lcTlHooks) << "Cannot query logind sessions; running prestart scripts:"
                            << reply.error().message();
        finish(false);
        return;
    }

    const auto sessions = reply.value();
    m_pendingSessionQueries = sessions.size();
    if (sessions.isEmpty()) {
        finish(false);
        return;
    }

    // Query session properties in parallel.
    for (const auto &session : sessions) {
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.login1"),
                                                    session.sessionPath.path(),
                                                    QStringLiteral("org.freedesktop.DBus.Properties"),
                                                    QStringLiteral("GetAll"));
        message << QStringLiteral("org.freedesktop.login1.Session");
        auto *propertiesWatcher = new QDBusPendingCallWatcher(
            QDBusConnection::systemBus().asyncCall(message), this);
        propertiesWatcher->setProperty("sessionId", session.sessionId);
        connect(propertiesWatcher, &QDBusPendingCallWatcher::finished,
                this, &LoginStateQuery::onSessionPropertiesQueried);
    }
}

void LoginStateQuery::onSessionPropertiesQueried(QDBusPendingCallWatcher *watcher)
{
    watcher->deleteLater();
    if (m_finished) {
        return;
    }

    const QDBusPendingReply<QVariantMap> reply = *watcher;
    if (reply.isError()) {
        qCWarning(lcTlHooks) << "Cannot query logind session properties:"
                            << watcher->property("sessionId").toString()
                            << reply.error().message();
    } else {
        const auto properties = reply.value();
        const auto type = properties.value(QStringLiteral("Type")).toString();
        const auto state = properties.value(QStringLiteral("State")).toString();
        if ((type == QLatin1String("wayland") || type == QLatin1String("x11"))
            && (state == QLatin1String("active") || state == QLatin1String("online"))) {
            qCInfo(lcTlHooks) << "Logind graphical session exists; skipping prestart scripts:"
                             << watcher->property("sessionId").toString() << type << state;
            finish(true);
            return;
        }
    }

    if (--m_pendingSessionQueries == 0) {
        finish(false);
    }
}

void LoginStateQuery::finish(bool hasLoggedInUser)
{
    if (m_finished) {
        return;
    }

    m_finished = true;
    Q_EMIT finished(hasLoggedInUser);
}
