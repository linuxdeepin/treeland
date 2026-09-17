// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/compositor-action/compositoractioninterfacev1.h"
#include "modules/show-desktop/showdesktopinterfacev1.h"
#include "seat/helper.h"
#include "server-bridge.h"
#include "workspace/workspace.h"

WAYLIB_SERVER_USE_NAMESPACE

namespace {
CompositorActionInterfaceV1 *g_compositorAction = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    g_compositorAction = find_server_interface<CompositorActionInterfaceV1>(helper);
}

extern "C" int compositor_action_global_exists(void *data)
{
    Q_UNUSED(data);
    return g_compositorAction != nullptr;
}

extern "C" void compositor_action_get_workspace_index(void *data)
{
    uint32_t *out = static_cast<uint32_t *>(data);
    auto *workspace = Helper::instance() ? Helper::instance()->workspace() : nullptr;
    *out = workspace ? static_cast<uint32_t>(workspace->currentIndex()) : UINT32_MAX;
}

extern "C" void compositor_action_get_show_desktop_state(void *data)
{
    uint32_t *out = static_cast<uint32_t *>(data);
    auto *helper = Helper::instance();
    *out = helper ? static_cast<uint32_t>(helper->showDesktopState()) : UINT32_MAX;
}
