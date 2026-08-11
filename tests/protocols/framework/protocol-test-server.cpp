// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "protocol-test-server.h"

#include <wbackend.h>
#include <woutput.h>
#include <wserver.h>

#include <drm/drm_fourcc.h>
#include <wlr_all.h>

#include <iterator>

WAYLIB_SERVER_USE_NAMESPACE

bool protocol_test_create_headless_output(WServer *server, int width, int height)
{
    auto *backend = server->findInterface<WBackend>();
    if (!backend)
        return false;

    if (!backend->handle()->start())
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

    wlr_output_create_global(output, server->handle());
    return WOutput::fromHandle(output) != nullptr;
}

bool protocol_test_enable_shm(WServer *server)
{
    static constexpr uint32_t formats[] = {
        DRM_FORMAT_ARGB8888,
        DRM_FORMAT_XRGB8888,
    };
    return wlr_shm_create(server->handle(), 1, formats, std::size(formats)) != nullptr;
}
