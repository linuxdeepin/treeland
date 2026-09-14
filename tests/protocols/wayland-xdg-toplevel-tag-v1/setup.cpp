// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-xdg-toplevel-tag-v1.h"
#include "server-bridge.h"
#include "core/shellhandler.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>
#include <wxdgtoplevelsurface.h>

#include <cstring>

namespace {
// The real mapped XdgToplevel SurfaceWrapper created by the client's
// xdg_toplevel_client_complete_map().
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

// E-level read: report the production WXdgToplevelSurface's tag.  Treeland's
// WXdgToplevelTagManagerV1 stores the client's set_toplevel_tag value here via
// WXdgToplevelSurface::setTag().
void xdg_toplevel_tag_read_server_state(void *data)
{
    auto *state = static_cast<struct xdg_toplevel_tag_server_state *>(data);
    state->valid = 0;
    state->tag[0] = '\0';

    if (!g_wrapper)
        return;

    auto *xdgSurface =
        qobject_cast<WXdgToplevelSurface *>(g_wrapper->shellSurface());
    if (!xdgSurface)
        return;

    state->valid = 1;
    const QByteArray utf8 = xdgSurface->tag().toUtf8();
    std::strncpy(state->tag, utf8.constData(), sizeof(state->tag) - 1);
    state->tag[sizeof(state->tag) - 1] = '\0';
}
