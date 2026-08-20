// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "server-bridge.h"
#include "seat/helper.h"

#include <wbackend.h>
#include <wlr_all.h>

namespace {
bool g_outputCreated = false;
}

void protocol_test_setup(Helper *helper)
{
    g_outputCreated = add_headless_output(helper->backend(), false);
    // This native global exists only for this protocol fixture.  Keeping it
    // out of Helper avoids making the production library depend on a protocol
    // implementation that is otherwise test-only.
    wlr_export_dmabuf_manager_v1_create(helper->backend()->server()->handle());
}

extern "C" bool protocol_test_skip()
{
#ifdef TREELAND_PROTOCOL_EXPECT_DMABUF_READY
    // A GLES2 GPU runner without a usable headless output cannot reach the
    // capability assertion in the C client. Mark that environment unsupported.
    return !g_outputCreated;
#else
    return false;
#endif
}
