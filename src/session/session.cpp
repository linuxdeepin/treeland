// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "session.h"
#include "sessionmanager.h"
#include "desktop/desktopenvironment.h"
#include "surface/surfacecontainer.h"
#include "sessionseatmanager.h"
#include "sessionworkspace.h"
#include "sessionscene.h"
#include "sessionprelaunch.h"
#include "sessionprotocols.h"
#include "sessionmultitaskview.h"
#include "sessioninputchannel.h"

#include "common/treelandlogging.h"
#include "core/shellhandler.h"
#include "core/waylandserver.h"
#include "workspace/workspace.h"
#ifndef DISABLE_XWAYLAND
#include "xsettings/settingmanager.h"
#endif

#include <wlr_all.h>

#include <wsocket.h>
#include <wforeigntoplevelv1.h>
#include <wserver.h>
#include "modules/foreign-toplevel/foreigntoplevelmanagerv3.h"
#include <wextforeigntoplevellistv1.h>
#ifndef DISABLE_XWAYLAND
#include <wxwayland.h>
#endif

void Session::detachInterface(WServerInterface *interface)
{
    if (!m_interfaces.removeOne(interface)) {
        return;
    }

    if (auto *server = WaylandServer::instance()) {
        server->server()->detach(interface);
    }

    delete interface;
}

Session::~Session()
{
    shutdown();
}

void Session::beginShutdown()
{
    if (m_shuttingDown)
        return;

    m_shuttingDown = true;

    if (m_scene)
        m_scene->shutdown();

    if (m_prelaunch)
        m_prelaunch->shutdown();
}

void Session::closeClients()
{
    if (!m_socket)
        return;

    QList<QPointer<WClient>> clients;
    QList<QPointer<WSocket>> sockets{m_socket};
    if (auto *runtime = WaylandServer::instance()) {
        wl_client *handle;
        wl_client_for_each(handle, wl_display_get_client_list(runtime->server()->handle())) {
            auto *client = WClient::get(handle);
            if (!ownsClient(client))
                continue;

            clients.append(client);
            for (auto *socket = client->socket(); socket && socket != m_socket;
                 socket = socket->parentSocket()) {
                if (!sockets.contains(socket))
                    sockets.append(socket);
            }
        }
    }

    for (const auto &socket : sockets) {
        if (socket)
            socket->stopListen();
    }

    // Destroy native resources while WClient still identifies their Session.
    // Native destruction removes each client from its socket before deleting WClient.
    // Snapshot QPointers because a resource callback may destroy another client.
    for (const auto &client : clients) {
        if (client && client->handle())
            wl_client_destroy(client->handle());
    }

    // WSocket::close() only closes its own clients, not descendant sockets.
    for (const auto &socket : sockets) {
        if (socket)
            socket->close();
    }
}

void Session::shutdown()
{
    if (m_shutdownStarted)
        return;

    m_shutdownStarted = true;
    beginShutdown();
    qCDebug(lcTlCore) << "Deleting session for uid:" << m_uid << m_socket;
    if (WaylandServer::instance()) {
        setActive(false);
    }

    Q_EMIT aboutToBeDestroyed();

#ifndef DISABLE_XWAYLAND
    if (m_settingManagerThread) {
        m_settingManagerThread->quit();
        m_settingManagerThread->wait(QDeadlineTimer(25000));
    }

    if (m_settingManager) {
        delete m_settingManager;
        m_settingManager = nullptr;
    }

    if (m_xwayland) {
        if (auto *server = WaylandServer::instance())
            server->shellHandler()->removeXWayland(m_xwayland);
        m_xwayland = nullptr;
    }
#endif

    closeClients();

    // Native surfaces have now been destroyed with their clients. Retire their
    // disabled wrappers before removing the protocols used by invalidation.
    if (m_scene)
        m_scene->finishShutdown();

    if (m_seatManager) {
        delete m_seatManager;
        m_seatManager = nullptr;
    }

    // Unwind dependencies in reverse attachment order. Resource callbacks have
    // finished, but keep the desktop alive while protocol wrappers detach.
    while (!m_interfaces.isEmpty())
        detachInterface(m_interfaces.constLast());

    if (m_prelaunch) {
        delete m_prelaunch;
        m_prelaunch = nullptr;
    }

    if (m_multitaskView) {
        delete m_multitaskView;
        m_multitaskView = nullptr;
    }

    if (m_desktop) {
        delete m_desktop->inputChannel();
        delete m_desktop;
        m_desktop = nullptr;
    }

    m_outputManager = nullptr;

    if (m_protocols) {
        delete m_protocols;
        m_protocols = nullptr;
    }

    if (m_scene) {
        delete m_scene;
        m_scene = nullptr;
    }

    if (m_workspace) {
        delete m_workspace;
        m_workspace = nullptr;
    }

    // Includes the output channel; no Session-owned timers or queued receivers
    // should survive just because somebody retains the Session's identity.
    const auto components = children();
    qDeleteAll(components);

    if (m_socket) {
        delete m_socket;
        m_socket = nullptr;
    }
}

Workspace *Session::workspace() const
{
    return m_workspace ? m_workspace->workspace() : nullptr;
}

void Session::setActive(bool active)
{
    if ((active && m_shuttingDown) || m_active == active)
        return;

    m_active = active;
    if (!active)
        Q_EMIT activeChanged(false);

    if (active)
        m_desktop->activate();
    else
        m_desktop->deactivate();

    if (active)
        Q_EMIT activeChanged(true);
}

bool Session::ownsClient(WClient *client) const
{
    auto *server = WaylandServer::instance();

    return server && server->sessionManager()->sessionForClient(client).get() == this;
}

void Session::attachInterface(WServerInterface *interface)
{
    if (m_shuttingDown) {
        delete interface;
        return;
    }

    if (m_interfaces.contains(interface))
        return;

    interface->setOwnsSocket(m_socket);
    if (!interface->server())
        WaylandServer::instance()->server()->attach(interface);

    if (auto *object = dynamic_cast<QObject *>(interface)) {
        object->setParent(WaylandServer::instance()->server());
        connect(object, &QObject::destroyed, this, [this, interface] {
            m_interfaces.removeOne(interface);
        });
    }

    m_interfaces.append(interface);
}

WForeignToplevel *Session::foreignToplevel() const
{
    return m_foreignToplevel;
}

WExtForeignToplevelListV1 *Session::toplevelList() const
{
    return m_toplevelList;
}

void Session::registerWindow(SurfaceWrapper *window)
{
    m_desktop->registerWindow(window);
}

void Session::unregisterWindow(SurfaceWrapper *window)
{
    m_desktop->unregisterWindow(window);
}
