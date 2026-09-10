// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-fractional-scale-v1.h"
#include "server-bridge.h"
#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "seat/helper.h"

#include <wbackend.h>
#include <woutput.h>

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
}

// E-level read: resolve the real WOutput that backs the headless output and
// return its fractional scale.  wlroots uses this exact value (multiplied by
// 120 and rounded) to populate the wp_fractional_scale_v1.preferred_scale
// event, so the client-side event must match the live production object.
void fractional_scale_read_server_state(void *data)
{
    auto *state = static_cast<struct fractional_scale_server_state *>(data);
    state->valid = 0;
    state->scale = 0.0f;

    auto *helper = Helper::instance();
    if (!helper)
        return;

    auto *container = helper->rootSurfaceContainer();
    if (!container)
        return;

    const auto &outputs = container->outputs();
    if (outputs.isEmpty())
        return;

    auto *woutput = outputs.first()->output();
    if (!woutput)
        return;

    state->valid = 1;
    state->scale = woutput->scale();
}
