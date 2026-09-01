// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-text-input-unstable-v3.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include <wbackend.h>
#include <protocols/private/wtextinputv3_p.h>

#include <wlr/types/wlr_text_input_v3.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WTextInputManagerV3 *g_mgr = nullptr;
WTextInputV3 *g_ti = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    g_mgr = find_server_interface<WTextInputManagerV3>(helper);
    if (g_mgr) {
        QObject::connect(g_mgr, &WTextInputManagerV3::newTextInput,
                         helper, [](WTextInputV3 *ti) {
                             g_ti = ti;
                         });
    }
}

void text_input_v3_read_server_state(void *data)
{
    auto *state = static_cast<struct text_input_v3_server_state *>(data);
    state->valid = 0;
    state->enabled = 0;

    if (!g_ti)
        return;

    state->valid = 1;
    state->enabled = g_ti->handle()->current_enabled ? 1 : 0;
}
