// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/output-manager/outputmanagement.h"
#include "server-bridge.h"
#include "seat/helper.h"

#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

void protocol_test_setup(Helper *helper)
{
    // get_color_control takes a wl_output argument; make a real headless
    // output so the client binds a genuine wl_output.
    add_headless_output(helper->backend(), false);
    Q_ASSERT(find_server_interface<OutputManagerV1>(helper));
}
