// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-text-input-unstable-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include <wbackend.h>
#include <protocols/private/wtextinputv1_p.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
WTextInputManagerV1 *g_mgr = nullptr;
bool g_activated = false;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    g_mgr = find_server_interface<WTextInputManagerV1>(helper);
    if (g_mgr) {
        QObject::connect(g_mgr, &WTextInputManagerV1::newTextInput,
                         helper, [](WTextInputV1 *ti) {
                             QObject::connect(ti, &WTextInputV1::activate,
                                              ti, []() {
                                                  g_activated = true;
                                              });
                         });
    }
}

void text_input_v1_read_server_state(void *data)
{
    auto *state = static_cast<struct text_input_v1_server_state *>(data);
    state->valid = g_mgr ? 1 : 0;
    state->activated = g_activated ? 1 : 0;
}
