// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-xdg-activation-v1.h"
#include "server-bridge.h"
#include "core/shellhandler.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "modules/activation/activationmanagerinterfacev1.h"

#include <wbackend.h>

namespace {
ActivationManagerInterfaceV1 *g_mgr = nullptr;

bool g_activate_requested = false;
int g_captured_disposition = 0;
WAYLIB_SERVER_NAMESPACE::WSurface *g_surface_ptr = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    g_mgr = find_server_interface<ActivationManagerInterfaceV1>(helper);
    if (g_mgr) {
        QObject::connect(g_mgr, &ActivationManagerInterfaceV1::activateRequested,
                         helper, [](ActivationManagerInterfaceV1::TokenDisposition disposition,
                                     WAYLIB_SERVER_NAMESPACE::WSurface *surface,
                                     WAYLIB_SERVER_NAMESPACE::WSeat *seat) {
                             (void)seat;
                             g_surface_ptr = surface;
                             g_activate_requested = true;
                             g_captured_disposition = static_cast<int>(disposition);
                         });
    }
}

void xdg_activation_read_server_state(void *data)
{
    auto *state = static_cast<struct xdg_activation_server_state *>(data);
    state->valid = g_activate_requested ? 1 : 0;
    state->surface_captured = g_surface_ptr ? 1 : 0;
    state->disposition = g_captured_disposition;
}
