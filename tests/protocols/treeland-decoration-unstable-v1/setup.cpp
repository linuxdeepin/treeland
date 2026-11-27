// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "server-bridge.h"
#include "treeland-decoration-unstable-v1.h"

#include "core/shellhandler.h"
#include "modules/decoration/decorationmanagerinterfacev1.h"
#include "core/waylandserver.h"
#include "dconfig/effectlevel.h"
#include "desktop/desktopenvironment.h"
#include "surface/surfacewrapper.h"

#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"

#include <wbackend.h>

namespace {
SurfaceWrapper *g_toplevel = nullptr;
}

void protocol_test_setup(WaylandServer *server)
{
    Q_ASSERT(find_server_interface<DecorationManagerInterfaceV1>(server));
    // Apply state geometry immediately: these cases assert configure sizes, not animation.
    if (auto *userConfig = server->config()) {
        userConfig->setMaximize(EffectLevel::None);
        userConfig->setFullscreen(EffectLevel::None);
        userConfig->setRestoreWindow(EffectLevel::None);
    }
    add_headless_output(server->backend(), false);
    QObject::connect(server->shellHandler(), &ShellHandler::surfaceWrapperAdded, server,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel)
                             g_toplevel = wrapper;
                     });
}

extern "C" void decoration_read_server_state(void *data)
{
    auto *state = static_cast<decoration_server_state *>(data);
    *state = {};
    if (!g_toplevel || !g_toplevel->desktop())
        return;

    state->valid = 1;
    state->radius = g_toplevel->radius();
    state->frame_radius = g_toplevel->frameGeometry().radius();
    state->presentation_radius = g_toplevel->presentationFrameGeometry().radius();
    state->shadow_visible = g_toplevel->shadowVisible();
    state->border_visible = g_toplevel->borderVisible();
    state->shadow_radius = g_toplevel->shadowBlurRadius();
    state->border_width = g_toplevel->borderWidth();
    const QRectF maximized = g_toplevel->maximizedGeometry();
    state->maximized_width = maximized.width();
    state->maximized_height = maximized.height();
    state->titlebar_height = g_toplevel->surfaceItem()->topPadding();
}

extern "C" void decoration_set_effect_switches(void *data)
{
    if (!g_toplevel || !g_toplevel->desktop())
        return;
    auto *config = g_toplevel->desktop()->config();
    if (!config)
        return;
    const auto *switches = static_cast<const decoration_effect_switches *>(data);
    config->setWindowCornerRadiusEnabled(
        switches->corner_radius ? EffectLevel::Medium : EffectLevel::None);
    config->setWindowShadowEnabled(
        switches->shadow ? EffectLevel::Medium : EffectLevel::None);
    config->setWindowBorderEnabled(
        switches->border ? EffectLevel::Medium : EffectLevel::None);
}

extern "C" void decoration_set_effect_level(void *data)
{
    auto *globalConfig = WaylandServer::instance()->globalConfig();
    if (!globalConfig)
        return;
    globalConfig->setEffectLevel(QString::fromUtf8(static_cast<const char *>(data)));
}
