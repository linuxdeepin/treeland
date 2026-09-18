// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-xdg-foreign-unstable-v2.h"
#include "server-bridge.h"
#include "core/shellhandler.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>
#include <wxdgtoplevelsurface.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
SurfaceWrapper *g_parentWrapper = nullptr;
SurfaceWrapper *g_childWrapper = nullptr;
int g_wrapperCount = 0;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    // Capture two XdgToplevel SurfaceWrappers: the first is the parent
    // (exported), the second is the child (set_parent_of target).
    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() != SurfaceWrapper::Type::XdgToplevel)
                             return;
                         if (g_wrapperCount == 0)
                             g_parentWrapper = wrapper;
                         else if (g_wrapperCount == 1)
                             g_childWrapper = wrapper;
                         ++g_wrapperCount;
                     });
}

void xdg_foreign_v2_read_server_state(void *data)
{
    auto *state = static_cast<struct xdg_foreign_v2_server_state *>(data);
    state->valid = 0;
    state->has_parent = 0;

    if (!g_parentWrapper || !g_childWrapper)
        return;

    state->valid = 1;

    // E-level: read back the real production WXdgToplevelSurface::parentXdgSurface()
    // on the child.  When the client calls set_parent_of via xdg-foreign-v2,
    // wlroots calls wlr_xdg_toplevel_set_parent which sets toplevel->parent and
    // emits events.set_parent.  WXdgToplevelSurface listens to this signal and
    // parentXdgSurface() reads handle()->parent directly.
    auto *shell = g_childWrapper->shellSurface();
    if (!shell)
        return;

    auto *toplevel = dynamic_cast<WXdgToplevelSurface *>(shell);
    if (!toplevel)
        return;

    state->has_parent = toplevel->parentXdgSurface() != nullptr;
}
