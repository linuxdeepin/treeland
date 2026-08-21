// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "server-bridge.h"

#include "seat/helper.h"

#include <wbackend.h>
#include <woutput.h>
#include <wserver.h>

#include <wlr_all.h>

WAYLIB_SERVER_USE_NAMESPACE

WServer *server_for_helper(Helper *helper)
{
    return helper ? helper->findChild<WServer *>() : nullptr;
}

bool add_headless_output(WServer *server, int width, int height)
{
    auto *backend = server->findInterface<WBackend>();
    return add_headless_output(backend, true, width, height);
}

bool add_headless_output(WBackend *backend, bool startBackend, int width, int height)
{
    if (!backend || (startBackend && !wlr_backend_start(backend->handle())))
        return false;

    auto *multi = backend->handle();
    if (!wlr_backend_is_multi(multi))
        return false;

    wlr_backend *headlessHandle = nullptr;
    wlr_multi_for_each_backend(multi, [](wlr_backend *backend, void *data) {
        if (wlr_backend_is_headless(backend))
            *static_cast<wlr_backend **>(data) = backend;
    }, &headlessHandle);
    if (!headlessHandle)
        return false;

    auto *output = wlr_headless_add_output(headlessHandle, width, height);
    if (!output)
        return false;

    auto *woutput = WOutput::fromHandle(output);
    if (!woutput)
        return false;
    wlr_output_create_global(output, woutput->server()->handle());
    return true;
}
