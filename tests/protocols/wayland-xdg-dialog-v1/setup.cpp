// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-xdg-dialog-v1.h"
#include "server-bridge.h"
#include "core/shellhandler.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>

namespace {
// The real mapped XdgToplevel SurfaceWrapper created by the client's
// xdg_toplevel_client_complete_map().  Captured from the production
// ShellHandler::surfaceWrapperAdded signal.
SurfaceWrapper *g_wrapper = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel)
                             g_wrapper = wrapper;
                     });
}

// E-level read: report whether the captured production SurfaceWrapper is modal.
// Treeland flips this via SurfaceWrapper::setModal() in response to the
// WXdgDialogManagerV1::surfaceModalChanged signal that fires on a client
// xdg_dialog_v1.set_modal request.
void xdg_dialog_read_server_state(void *data)
{
    auto *state = static_cast<struct xdg_dialog_server_state *>(data);
    state->valid = (g_wrapper != nullptr);
    state->modal = (g_wrapper && g_wrapper->modal()) ? 1 : 0;
}
