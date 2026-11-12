// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "session.h"
#include "sessionmanager.h"
#include "desktop/desktopenvironment.h"
#include "surface/surfacecontainer.h"
#include "sessionseatmanager.h"

#include "common/treelandlogging.h"
#include "core/shellhandler.h"
#include "core/waylandserver.h"
#include "workspace/workspace.h"
#ifndef DISABLE_XWAYLAND
#include "xsettings/settingmanager.h"
#endif

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
        delete interface;
    }
}

Session::~Session()
{
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

    if (m_socket)
        m_socket->close();

    if (m_seatManager) {
        delete m_seatManager;
        m_seatManager = nullptr;
    }

    if (auto *server = WaylandServer::instance()) {
        for (auto *interface : std::as_const(m_interfaces)) {
            server->server()->detach(interface);
            delete interface;
        }
    }

    delete m_socket;
    m_socket = nullptr;
    auto *ownedWorkspace = workspace();
    if (ownedWorkspace && WaylandServer::instance()
        && WaylandServer::instance()->workspace() != ownedWorkspace) {
        auto *workspace = ownedWorkspace;
        workspace->deactivate();
        if (workspace->surfaces().isEmpty()) {
            workspace->deleteLater();
        } else {
            connect(workspace, &SurfaceContainer::surfaceRemoved, workspace, [workspace] {
                if (workspace->surfaces().isEmpty())
                    workspace->deleteLater();
            });
        }
    }
}

void Session::setActive(bool active)
{
    if (m_active == active)
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
    if (m_interfaces.contains(interface))
        return;

    interface->setOwnsSocket(m_socket);
    if (!interface->server())
        WaylandServer::instance()->server()->attach(interface);

    if (auto *object = dynamic_cast<QObject *>(interface))
        object->setParent(WaylandServer::instance()->server());

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
