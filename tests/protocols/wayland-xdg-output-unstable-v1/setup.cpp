// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-xdg-output-unstable-v1.h"
#include "server-bridge.h"
#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "seat/helper.h"

#include <wbackend.h>
#include <woutput.h>

void protocol_test_setup(Helper *helper)
{
    // A real headless output is committed with a mode (1920x1080) so that
    // geometry-dependent events (logical_position / logical_size / done) are
    // actually delivered to the client.  The same WOutput is read back below
    // to cross-check those events against the live production object.
    add_headless_output(helper->backend(), false);
}

// E-level read: resolve the real WOutput that backs the headless output and
// return its layout geometry.  This reads the identical wlroots state that
// WXdgOutputManager uses to populate logical_position / logical_size, so the
// client-side protocol events must match these values exactly.
void xdg_output_read_server_state(void *data)
{
    auto *state = static_cast<struct xdg_output_server_state *>(data);
    state->valid = 0;

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
    state->x = woutput->position().x();
    state->y = woutput->position().y();
    state->width = woutput->effectiveSize().width();
    state->height = woutput->effectiveSize().height();
}
