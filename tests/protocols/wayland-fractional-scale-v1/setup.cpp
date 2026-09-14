// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-fractional-scale-v1.h"
#include "server-bridge.h"
#include "seat/helper.h"

#include <wbackend.h>
#include <woutput.h>

#include <QtCore/QPointer>
#include <QtCore/qglobal.h>

static QPointer<WOutput> g_woutput;

void protocol_test_setup(Helper *helper)
{
    auto *output = add_headless_output(helper->backend(), false);
    Q_ASSERT(output);
    g_woutput = output;
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

    WOutput *woutput = g_woutput.data();
    if (!woutput)
        return;

    state->valid = 1;
    state->scale = woutput->scale();
}
