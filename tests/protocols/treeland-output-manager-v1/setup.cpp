// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/output-manager/outputmanagement.h"
#include "seat/helper.h"

#include <wbackend.h>
#include <wserver.h>

#include <qwbackend.h>
#include <qwdisplay.h>

#include <wlr/backend.h>
#include <wlr/types/wlr_output.h>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

namespace {

/* The test server's WBackend is a wlr multi backend wrapping a headless
 * backend (WLR_BACKENDS=headless). Find the headless child so we can add a
 * real, bindable output to it. */
void createTestOutput(WServer *server)
{
    auto *backend = server->findInterface<WBackend>();
    if (!backend)
        return;

    /* The headless backend only reports outputs once started (production calls
     * wlr_backend_start in src/seat/helper.cpp); without this, add_output
     * never emits new_output and no WOutput wrapper is created, so the client's
     * wl_output cannot be resolved server-side. */
    backend->handle()->start();

    auto *multi = qw_multi_backend::from(backend->handle()->handle());
    if (!multi)
        return;

    wlr_backend *headlessHandle = nullptr;
    multi->for_each_backend([](wlr_backend *b, void *data) {
        if (wlr_backend_is_headless(b))
            *static_cast<wlr_backend **>(data) = b;
    }, &headlessHandle);
    if (!headlessHandle)
        return;

    auto *headless = qw_headless_backend::from(headlessHandle);
    wlr_output *output = headless->add_output(1920, 1080);
    if (!output)
        return;

    /* wlroots 0.19: wlr_output_create_global() takes the wl_display as a
     * second argument; the qwlroots wrapper does not match this signature,
     * so call the C API directly. */
    wlr_output_create_global(output, server->handle()->handle());
}

}

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
    createTestOutput(server);

    server->attach<OutputManagerV1>();
}
