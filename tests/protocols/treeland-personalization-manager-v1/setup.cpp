// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/personalization/personalizationmanagerinterfacev1.h"
#include "seat/helper.h"
#include "treeland-personalization-manager-v1.h"

#include <wserver.h>

#include <cstring>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
PersonalizationWindowContextV1 *g_windowContext = nullptr;
}

void protocol_test_setup(WServer *server)
{
    // PersonalizationManagerInterfaceV1 serves cursor/font/appearance contexts from
    // Helper::instance()->config() (a TreelandUserConfig): creating any of those
    // contexts dereferences the Helper singleton, so a Helper must exist before a
    // client can use them.
    if (!Helper::instance()) {
        new Helper(server);
    }

    auto *manager = server->attach<PersonalizationManagerInterfaceV1>();
    QObject::connect(manager, &PersonalizationManagerInterfaceV1::windowContextCreated,
                     [](PersonalizationWindowContextV1 *context) { g_windowContext = context; });
}

extern "C" void personalization_window_state(void *data)
{
    auto *state = static_cast<struct window_context_state *>(data);
    std::memset(state, 0, sizeof(*state));
    if (!g_windowContext)
        return;

    state->background_type = g_windowContext->backgroundType();
    state->corner_radius = g_windowContext->cornerRadius();

    const Shadow shadow = g_windowContext->shadow();
    state->shadow_radius = shadow.radius;
    state->shadow_offset_x = shadow.offset.x();
    state->shadow_offset_y = shadow.offset.y();
    state->shadow_red = shadow.color.red();
    state->shadow_green = shadow.color.green();
    state->shadow_blue = shadow.color.blue();
    state->shadow_alpha = shadow.color.alpha();

    const Border border = g_windowContext->border();
    state->border_width = border.width;
    state->border_red = border.color.red();
    state->border_green = border.color.green();
    state->border_blue = border.color.blue();
    state->border_alpha = border.color.alpha();

    state->no_titlebar =
        g_windowContext->states().testFlag(PersonalizationWindowContextV1::NoTitleBar) ? 1 : 0;
}
