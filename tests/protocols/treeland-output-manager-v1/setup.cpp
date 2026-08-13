// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/output-manager/outputmanagement.h"
#include "protocol-test-server.h"
#include "seat/helper.h"

#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

void protocol_test_setup(WServer *server)
{
    // OutputManagerV1 routes every request and event through the compositor's
    // Helper singleton (Helper::instance()->rootSurfaceContainer() and
    // Helper::instance()->getOutput()): the manager's bind handler
    // dereferences it, so a Helper instance must exist before any client can
    // bind the manager.
    if (!Helper::instance())
        new Helper(server);

    // get_color_control takes a wl_output argument; make a real headless
    // output so the client binds a genuine wl_output.
    protocol_test_create_headless_output(server);

    server->attach<OutputManagerV1>();
}
