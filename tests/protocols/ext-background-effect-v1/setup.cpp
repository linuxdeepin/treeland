// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "ext-background-effect-v1.h"
#include "server-bridge.h"
#include "core/shellhandler.h"
#include "core/waylandserver.h"
#include "dconfig/effectlevel.h"
#include "desktop/desktopenvironment.h"
#include "surface/surfacewrapper.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"

#include <wbackend.h>

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

// E-level read: report the captured production SurfaceWrapper's committed
// ext-background-effect-v1 blur state, synced from the protocol surface
// state on every wl_surface.commit. A non-empty blur region enables full
// surface blur.
void background_effect_read_server_state(void *data)
{
    auto *state = static_cast<struct background_effect_server_state *>(data);
    state->valid = 0;
    state->blur_enabled = 0;

    if (!g_wrapper)
        return;

    state->valid = 1;
    state->blur_enabled = g_wrapper->blur() ? 1 : 0;
}

void background_effect_set_enabled(void *data)
{
    if (!g_wrapper || !g_wrapper->desktop())
        return;
    auto *config = g_wrapper->desktop()->config();
    if (!config)
        return;
    config->setBackgroundEffectEnabled(
        *static_cast<const int *>(data) != 0 ? EffectLevel::Medium : EffectLevel::None);
}

void background_effect_set_level(void *data)
{
    auto *globalConfig = WaylandServer::instance()->globalConfig();
    if (!globalConfig)
        return;
    globalConfig->setEffectLevel(QString::fromUtf8(static_cast<const char *>(data)));
}
