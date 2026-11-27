// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-alpha-modifier-v1.h"
#include "server-bridge.h"
#include "core/shellhandler.h"
#include "core/waylandserver.h"
#include "dconfig/effectlevel.h"
#include "desktop/desktopenvironment.h"
#include "surface/surfacewrapper.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"

#include <QQuickItem>
#include <wbackend.h>
#include <wlr_all.h>
#include <wsurface.h>
#include <wsurfaceitem.h>

namespace {
// The real mapped XdgToplevel SurfaceWrapper created by the client.
SurfaceWrapper *g_wrapper = nullptr;
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);

    QObject::connect(server->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     server,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel)
                             g_wrapper = wrapper;
                     });
}

// E-level read: report the captured production SurfaceWrapper's alpha-modifier
// multiplier.  wlroots stores this as a double in [0, 1], updated on commit
// when the client calls wp_alpha_modifier_surface_v1.set_multiplier.
void alpha_modifier_read_server_state(void *data)
{
    auto *state = static_cast<struct alpha_modifier_server_state *>(data);
    state->valid = 0;
    state->has_modifier = 0;
    state->multiplier = 0.0;
    state->has_content_item = 0;
    state->content_opacity = 0.0;
    state->has_item_content = 0;
    state->item_content_opacity = 0.0;

    if (!g_wrapper)
        return;

    auto *wsurface = g_wrapper->surface();
    if (!wsurface)
        return;

    state->valid = 1;
    if (auto *surfaceItem = g_wrapper->surfaceItem()) {
        if (auto *contentItem = surfaceItem->contentItem()) {
            state->has_content_item = 1;
            state->content_opacity = contentItem->opacity();
        }
        if (auto *itemContent = surfaceItem->findItemContent()) {
            state->has_item_content = 1;
            state->item_content_opacity = itemContent->opacity();
        }
    }

    auto *wlr_surf = wsurface->handle();
    if (!wlr_surf)
        return;

    const auto *am_state = wlr_alpha_modifier_v1_get_surface_state(wlr_surf);
    if (am_state) {
        state->has_modifier = 1;
        state->multiplier = am_state->multiplier;
    }
}

void alpha_modifier_set_transparency_enabled(void *data)
{
    if (!g_wrapper || !g_wrapper->desktop())
        return;
    auto *config = g_wrapper->desktop()->config();
    if (!config)
        return;
    config->setWindowTransparencyEnabled(
        *static_cast<const int *>(data) != 0 ? EffectLevel::Medium : EffectLevel::None);
}

void alpha_modifier_set_effect_level(void *data)
{
    auto *globalConfig = WaylandServer::instance()->globalConfig();
    if (!globalConfig)
        return;
    globalConfig->setEffectLevel(QString::fromUtf8(static_cast<const char *>(data)));
}
