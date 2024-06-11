// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquicksocket_p.h"
#include "wsocket.h"

#include <QQmlInfo>

WAYLIB_SERVER_BEGIN_NAMESPACE

WQuickSocketAttached::WQuickSocketAttached(WSocket *socket)
    : QObject(socket)
{

}

WSocket *WQuickSocketAttached::socket() const
{
    return qobject_cast<WSocket*>(parent());
}

WSocket *WQuickSocketAttached::rootSocket() const
{
    return socket()->rootSocket();
}

WQuickSocket::WQuickSocket(QObject *parent)
    : WQuickWaylandServerInterface(parent)
{

}

WQuickSocketAttached *WQuickSocket::qmlAttachedProperties(QObject *target)
{
    if (auto wobject = dynamic_cast<WObject*>(target)) {
        auto client = wobject->waylandClient();
        if (client) {
            auto socket = client->socket();
            Q_ASSERT(socket);
            auto attached = socket->findChild<WQuickSocketAttached*>(QString(), Qt::FindDirectChildrenOnly);

            if (!attached)
                attached = new WQuickSocketAttached(socket);

            return attached;
        }
    }

    return nullptr;
}

WSocket *WQuickSocket::socket() const
{
    return m_socket;
}

void WQuickSocket::addClient(wl_client *client)
{
    if (!m_socket) {
        qmlWarning(this) << "Can't add client, WSocket is null";
        return;
    }
    m_socket->addClient(client);
}

void WQuickSocket::removeClient(wl_client *client)
{
    if (!m_socket) {
        qmlWarning(this) << "Can't remove client, WSocket is null";
        return;
    }
    m_socket->removeClient(client);
}

void WQuickSocket::removeClient(WClient *client)
{
    if (!m_socket) {
        qmlWarning(this) << "Can't remove client, WSocket is null";
        return;
    }
    m_socket->removeClient(client);
}

QString WQuickSocket::socketFile() const
{
    return m_socket ? m_socket->fullServerName() : nullptr;
}

void WQuickSocket::setSocketFile(const QString &newSocketFile)
{
    if (m_socketFile == newSocketFile)
        return;
    m_socketFile = newSocketFile;

    if (!isPolished())
        return;

    createSocket(newSocketFile);
}

bool WQuickSocket::enabled() const
{
    return m_enabled;
}

void WQuickSocket::setEnabled(bool newEnabled)
{
    if (m_enabled == newEnabled)
        return;
    m_enabled = newEnabled;

    if (m_socket)
        m_socket->setEnabled(m_enabled);

    Q_EMIT enabledChanged();
}

bool WQuickSocket::freezeClientWhenDisable() const
{
    return m_freezeClientWhenDisable;
}

void WQuickSocket::setFreezeClientWhenDisable(bool newFreezeClientWhenDisable)
{
    if (m_freezeClientWhenDisable == newFreezeClientWhenDisable)
        return;

    if (isPolished())
        return;

    Q_ASSERT(!m_socket);
    m_freezeClientWhenDisable = newFreezeClientWhenDisable;
    Q_EMIT freezeClientWhenDisableChanged();
}

void WQuickSocket::polish()
{
    if (m_socketFile.isEmpty()) {
        Q_ASSERT(!m_socket);
        auto socket = new WSocket(m_freezeClientWhenDisable);
        if (socket->autoCreate()) {
            setSocket(socket);
        } else {
            delete socket;
        }
    } else {
        createSocket(m_socketFile);
    }
}

void WQuickSocket::setSocket(WSocket *socket)
{
    Q_ASSERT(m_socket != socket);
    if (m_socket)
        m_socket->deleteLater();

    m_socket = socket;

    if (m_socket) {
        m_socket->setParent(this);
        m_socket->setEnabled(m_enabled);
        server()->addSocket(m_socket);
    }

    Q_EMIT socketChanged();
    Q_EMIT socketFileChanged();
}

void WQuickSocket::createSocket(const QString &file)
{
    auto socket = new WSocket(m_freezeClientWhenDisable);
    if (!socket->create(file)) {
        delete socket;
        socket = nullptr;
    }

    setSocket(socket);
}

WAYLIB_SERVER_END_NAMESPACE
