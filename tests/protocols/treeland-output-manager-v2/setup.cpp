// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/output-manager/outputmanagement.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "core/rootsurfacecontainer.h"
#include "output.h"

#include <wserver.h>
#include <wlr_all.h>

WAYLIB_SERVER_USE_NAMESPACE

void protocol_test_setup(WaylandServer *server)
{
    // v2 addresses outputs by uuid; the client needs a real wl_output to ask
    // treeland-output-uuid-v1 for that uuid.
    add_headless_output(server->backend(), false);
    Q_ASSERT(find_server_interface<OutputManagerV2>(server));
}

// Disables the first output so a client holding its wl_output can exercise the
// non-fatal primary_output_failed path.
extern "C" void protocol_test_disable_output()
{
    auto *server = WaylandServer::instance();
    const auto outputs = server->rootSurfaceContainer()->outputs();
    if (outputs.isEmpty())
        return;

    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, false);
    wlr_output_commit_state(outputs.first()->output()->handle(), &state);
    wlr_output_state_finish(&state);
}
