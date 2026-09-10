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

    // wlroots 0.20 requires an explicit commit to enable the output and apply
    // its mode.  Without this, geometry/rendering-dependent events (xdg-output,
    // session-lock, image-copy-capture) are never delivered to clients.
    struct wlr_output_state outputState;
    wlr_output_state_init(&outputState);
    wlr_output_state_set_enabled(&outputState, true);
    wlr_output_state_set_custom_mode(&outputState, width, height, 0);
    wlr_output_commit_state(output, &outputState);
    wlr_output_state_finish(&outputState);

    auto *woutput = WOutput::fromHandle(output);
    if (!woutput)
        return false;
    wlr_output_create_global(output, woutput->server()->handle());
    return true;
}
