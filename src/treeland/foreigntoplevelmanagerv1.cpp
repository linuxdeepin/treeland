// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR
// GPL-3.0-only

#include "foreigntoplevelmanagerv1.h"

#include <qwdisplay.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include <QDebug>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <QTimer>

#include "protocols/foreign_toplevel_manager_impl.h"
#include "foreign-toplevel-manager-server-protocol.h"

ForeignToplevelManager::ForeignToplevelManager(QObject *parent)
    : Waylib::Server::WQuickWaylandServerInterface(parent)
    , m_impl(new ztreeland_foreign_toplevel_manager_v1)
{
}

ztreeland_foreign_toplevel_manager_v1 *ForeignToplevelManager::impl() {
    return m_impl;
}

void ForeignToplevelManager::setHelper(TreeLandHelper *helper) {
    m_helper = helper;
}

void ForeignToplevelManager::create()
{
    m_impl->manager = this;

    m_impl->global = wl_global_create(
        server()->handle()->handle(), &ztreeland_foreign_toplevel_manager_v1_interface,
        ZTREELAND_FOREIGN_TOPLEVEL_MANAGER_V1_TOPLEVEL, m_impl, foreign_toplevel_manager_bind);

    wl_list_init(&m_impl->contexts);

    wl_signal_init(&m_impl->events.destroy);

    // TODO: I think it isn't need.
    m_impl->display_destroy.notify = foreign_toplevel_manager_handle_display_destroy;
    wl_display_add_destroy_listener(server()->handle()->handle(),
                                    &m_impl->display_destroy);
}
