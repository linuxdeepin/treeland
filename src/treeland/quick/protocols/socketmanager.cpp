// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "socketmanager.h"

#include <qwdisplay.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wserver.h>

#include <QDebug>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <QTimer>

#include "socket_manager_impl.h"

SocketManager::SocketManager(QObject *parent)
    : Waylib::Server::WQuickWaylandServerInterface(parent)
    , m_impl(new treeland_socket_manager_v1)
{
}

treeland_socket_manager_v1 *SocketManager::impl()
{
    return m_impl;
}

void SocketManager::create()
{
    m_impl->manager = this;

    m_impl->global = wl_global_create(
        server()->handle()->handle(), &treeland_socket_manager_v1_interface,
        TREELAND_SOCKET_MANAGER_V1_VERSION, m_impl, socket_manager_bind);

    wl_list_init(&m_impl->contexts);

    wl_signal_init(&m_impl->events.destroy);
    wl_signal_init(&m_impl->events.new_user_socket);

    m_impl->display_destroy.notify = socket_manager_handle_display_destroy;
    wl_display_add_destroy_listener(server()->handle()->handle(),
                                    &m_impl->display_destroy);
}
