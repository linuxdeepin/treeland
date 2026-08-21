// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/shellhandler.h"
#include "core/rootsurfacecontainer.h"
#include "modules/personalization/personalizationmanagerinterfacev1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "treeland-personalization-desktop-v1.h"
#include "workspace/workspace.h"

#include <wbackend.h>

namespace {
SurfaceWrapper *g_wrapper = nullptr;
personalization_desktop_state g_state {};
}

void protocol_test_setup(Helper *helper)
{
    // Helper has finished its initial backend scan.  add_output() notifies the
    // production WBackend path asynchronously, so readiness is checked by the
    // desktop main loop rather than from this immediate return value.
    add_headless_output(helper->backend(), false);
    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [helper](SurfaceWrapper *wrapper) {
                         if (wrapper->type() != SurfaceWrapper::Type::XdgToplevel)
                             return;
                         g_wrapper = wrapper;
                         g_state.wrapper_created = 1;
                         g_state.wrapper_in_workspace = helper->workspace()->surfaces().contains(wrapper) ? 1 : 0;
                     });
}

extern "C" void personalization_desktop_read_state(void *data)
{
    auto state = g_state;
    state.output_ready = !Helper::instance()->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
    if (g_wrapper) {
        state.corner_radius = static_cast<int>(g_wrapper->radius());
        state.blur = g_wrapper->blur() ? 1 : 0;
        state.wrapper_no_titlebar = g_wrapper->noTitleBar() ? 1 : 0;
        if (auto *personalization = g_wrapper->findChild<Personalization *>()) {
            state.personalization_attached = 1;
            state.background_type = static_cast<int>(personalization->backgroundType());
            state.no_titlebar = personalization->noTitlebar() ? 1 : 0;
            const Shadow shadow = personalization->shadow();
            state.shadow_radius = shadow.radius;
            state.shadow_offset_x = shadow.offset.x();
            state.shadow_offset_y = shadow.offset.y();
            state.shadow_red = shadow.color.red();
            state.shadow_green = shadow.color.green();
            state.shadow_blue = shadow.color.blue();
            state.shadow_alpha = shadow.color.alpha();
            const Border border = personalization->border();
            state.border_width = border.width;
            state.border_red = border.color.red();
            state.border_green = border.color.green();
            state.border_blue = border.color.blue();
            state.border_alpha = border.color.alpha();
        }
    }
    *static_cast<personalization_desktop_state *>(data) = state;
}
