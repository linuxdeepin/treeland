// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocol-test-server.h"
#include "modules/screensaver/screensaverinterfacev1.h"
#include "seat/helper.h"

#include <wbackend.h>
#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
ScreensaverInterfaceV1 *g_screensaver = nullptr;
}

void protocol_test_desktop_setup(Helper *helper)
{
    protocol_test_create_headless_output(helper->backend(), false);
    g_screensaver = helper->backend()->server()->findInterface<ScreensaverInterfaceV1>();
    Q_ASSERT(g_screensaver);
}

/*
 * Server-side state hook called from the pure-C client via
 * protocol_test_invoke_server. Runs on the compositor's Qt thread.
 *
 * Reads the module's public isInhibited() state so the client can assert the
 * effect of its own inhibit/uninhibit requests. This is a state query, not a
 * faked request result: the requests themselves do the real work.
 */
extern "C" void screensaver_query_inhibited(void *data)
{
    int *inhibited = static_cast<int *>(data);
    *inhibited = (g_screensaver && g_screensaver->isInhibited()) ? 1 : 0;
}
