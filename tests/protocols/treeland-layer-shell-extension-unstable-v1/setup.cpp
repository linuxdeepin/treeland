// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-3.0-only
#include "modules/layer-shell-extension/layershellextensionmanagerinterfacev1.h"
#include "core/waylandserver.h"
#include "server-bridge.h"

#include <wbackend.h>

void protocol_test_setup(WaylandServer *server)
{
    Q_ASSERT(find_server_interface<LayerShellExtensionManagerInterfaceV1>(server));
    add_headless_output(server->backend(), false);
}
