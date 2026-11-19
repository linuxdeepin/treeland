// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "socket-lifecycle.h"
#include "core/waylandserver.h"
#include "greeter/usermodel.h"
#include "server-bridge-api.h"
#include "server-bridge.h"
#include "session/session.h"
#include "session/sessionmanager.h"

#include <wsocket.h>
#include <wayland-server-core.h>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusUnixFileDescriptor>
#include <QPointer>
#include <QSemaphore>
#include <QTemporaryDir>

#include <memory>
#include <pwd.h>
#include <unistd.h>

namespace {
const QString ownerNames[] = {
    QStringLiteral("session-socket-first-owner"),
    QStringLiteral("session-socket-second-owner"),
};

struct SocketFixture {
    QTemporaryDir directory;
    WSocket original{false};
    WSocket replacement{false};
    std::unique_ptr<WSocket> descendant;
    std::shared_ptr<Session> session;
    QPointer<WSocket> registeredSocket;
};

std::unique_ptr<SocketFixture> fixture;

bool createEndpoint(WSocket &socket, const QString &path, socket_endpoint &endpoint)
{
    if (!socket.create(path))
        return false;
    qstrncpy(endpoint.name, path.toUtf8().constData(), sizeof(endpoint.name));
    endpoint.fd = socket.socketFd();
    return true;
}

struct OwnerExit {
    QString service;
    QSemaphore observed;
};

void watchOwnerExit(void *data)
{
    const auto exit = *static_cast<std::shared_ptr<OwnerExit> *>(data);
    auto *watcher = new QDBusServiceWatcher(exit->service, QDBusConnection::systemBus(),
        QDBusServiceWatcher::WatchForUnregistration, WaylandServer::instance());
    QObject::connect(watcher, &QDBusServiceWatcher::serviceUnregistered, watcher,
        [watcher, exit] {
            watcher->deleteLater();
            exit->observed.release();
        }, Qt::QueuedConnection);
}
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);
}

extern "C" void socket_lifecycle_prepare(void *data)
{
    auto &state = *static_cast<socket_lifecycle_state *>(data);
    auto *server = WaylandServer::instance();
    auto *manager = server->sessionManager();
    const auto *account = getpwuid(getuid());
    if (!account) {
        state.ready = -1;
        return;
    }
    const auto username = QString::fromLocal8Bit(account->pw_name);
    if (username == manager->globalSession()->username())
        return;

    fixture = std::make_unique<SocketFixture>();
    fixture->session = manager->ensureSession(QStringLiteral("protocol-session-socket-lifecycle"),
                                              username);
    if (!fixture->session || manager->sessionForUid(getuid()) != fixture->session
        || !fixture->directory.isValid()
        || !createEndpoint(fixture->original, fixture->directory.filePath(QStringLiteral("original")), state.original)
        || !createEndpoint(fixture->replacement, fixture->directory.filePath(QStringLiteral("replacement")), state.replacement)) {
        state.ready = -1;
        return;
    }
    fixture->session->socket()->setEnabled(true);
    state.ready = 1;
}

extern "C" int socket_lifecycle_register(socket_owner owner, int fd)
{
    const auto &name = ownerNames[owner];
    auto bus = QDBusConnection(name);
    if (!bus.isConnected())
        bus = QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SYSTEM_BUS_ADDRESS"), name);
    if (!bus.isConnected())
        return 0;

    auto request = QDBusMessage::createMethodCall(QStringLiteral("org.deepin.Compositor1"),
        QStringLiteral("/org/deepin/Compositor1"), QStringLiteral("org.deepin.Compositor1"),
        QStringLiteral("ActivateWayland"));
    request << QVariant::fromValue(QDBusUnixFileDescriptor(fd));
    const QDBusReply<bool> reply = bus.call(request);
    return reply.isValid() && reply.value();
}

extern "C" void socket_lifecycle_check_enable_sync(void *data)
{
    auto &state = *static_cast<socket_lifecycle_state *>(data);
    state.enable_synced = 0;
    if (!fixture || !fixture->session)
        return;

    auto *userModel = WaylandServer::instance()->userModel();
    const auto account = userModel ? userModel->getUser(fixture->session->username()) : nullptr;
    const auto socket = account ? account->waylandSocket() : nullptr;
    if (!socket || !socket->clients().isEmpty() || !socket->isEnabled())
        return;

    fixture->session->socket()->setEnabled(false);
    const bool disabledWithSession = !socket->isEnabled();
    fixture->session->socket()->setEnabled(true);
    const bool enabledWithSession = socket->isEnabled();

    // The greeter showing another user must not disable this Session's listener.
    // The fixture user is never "dde", so the removed UserModel loop would disable it.
    const QString previousUser = userModel->currentUserName();
    userModel->setCurrentUserName(QStringLiteral("dde"));
    const bool unaffectedByCurrentUser = socket->isEnabled();
    userModel->setCurrentUserName(previousUser);

    state.enable_synced = disabledWithSession && enabledWithSession && unaffectedByCurrentUser;
}

extern "C" int socket_lifecycle_disconnect_owner(socket_owner owner)
{
    auto exit = std::make_shared<OwnerExit>();
    const auto &name = ownerNames[owner];
    exit->service = QDBusConnection(name).baseService();
    if (exit->service.isEmpty() || !invoke_on_server_thread(watchOwnerExit, &exit))
        return 0;
    QDBusConnection::disconnectFromBus(name);
    return exit->observed.tryAcquire(1, 5000) ? 1 : 0;
}

extern "C" void socket_lifecycle_add_descendant(void *data)
{
    auto &state = *static_cast<socket_lifecycle_state *>(data);
    auto *server = WaylandServer::instance();
    const auto listenerName = fixture->replacement.fullServerName();
    wl_client *client;
    wl_client_for_each(client, wl_display_get_client_list(server->server()->handle())) {
        auto *socket = WSocket::get(client);
        if (socket && socket->parentSocket() == fixture->session->socket()
            && socket->fullServerName() == listenerName) {
            fixture->registeredSocket = socket;
            break;
        }
    }
    if (!fixture->registeredSocket)
        return;

    // A descendant listener exercises the same socket ownership lookup as security-context.
    fixture->descendant = std::make_unique<WSocket>(fixture->registeredSocket.data());
    const auto path = fixture->directory.filePath(QStringLiteral("descendant"));
    if (!fixture->descendant->create(path))
        return;
    server->addSocket(fixture->descendant.get());
    qstrncpy(state.descendant, path.toUtf8().constData(), sizeof(state.descendant));
}

extern "C" void socket_lifecycle_logout(void *data)
{
    auto &state = *static_cast<socket_lifecycle_state *>(data);
    WaylandServer::instance()->sessionManager()->removeSession(fixture->session);
    state.released = fixture->registeredSocket.isNull();
}

extern "C" void socket_lifecycle_cleanup(void *)
{
    if (fixture && fixture->session)
        WaylandServer::instance()->sessionManager()->removeSession(fixture->session);
    fixture.reset();
    for (const auto &name : ownerNames)
        QDBusConnection::disconnectFromBus(name);
}
