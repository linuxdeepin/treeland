// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-xdg-output-unstable-v1.h"
#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "seat/helper.h"

#include <woutput.h>
#include <woutputlayout.h>

#include <QtCore/QPointer>
#include <QtCore/qglobal.h>

static QPointer<WOutput> g_woutput;

void protocol_test_setup(Helper *helper)
{
    // The protocol test backend already provides HEADLESS-1. Reuse that
    // stable production output instead of changing display-topology config
    // merely to create another output for this test.
    auto *output = helper->rootSurfaceContainer()->primaryOutput();
    Q_ASSERT(output);
    g_woutput = output->output();
    Q_ASSERT(g_woutput);
}

extern "C" bool protocol_test_ready(Helper *helper)
{
    return g_woutput
        && helper->rootSurfaceContainer()->outputLayout()->outputs().contains(g_woutput);
}

// E-level read: resolve the real WOutput that backs the headless output and
// return its layout geometry.  This reads the identical wlroots state that
// WXdgOutputManager uses to populate logical_position / logical_size, so the
// client-side protocol events must match these values exactly.
void xdg_output_read_server_state(void *data)
{
    auto *state = static_cast<struct xdg_output_server_state *>(data);
    state->valid = 0;

    WOutput *woutput = g_woutput.data();
    if (!woutput)
        return;

    state->valid = 1;
    state->x = woutput->position().x();
    state->y = woutput->position().y();
    state->width = woutput->effectiveSize().width();
    state->height = woutput->effectiveSize().height();
}
