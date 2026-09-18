// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/region-watch/regionwatchmanagerinterfacev1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "treeland-region-watch-unstable-v1.h"

#include "core/rootsurfacecontainer.h"
#include "output/output.h"

#include <wbackend.h>
#include <woutput.h>
#include <wserver.h>

#include <wlr_all.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
QPointer<RegionWatchV1> g_watch;
TreelandRegionWatchManagerInterfaceV1 *g_manager = nullptr;
wlr_output *g_secondOutput = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    g_manager = find_server_interface<TreelandRegionWatchManagerInterfaceV1>(helper);
    Q_ASSERT(g_manager);

    // A second output whose removal the client can observe via output_removed.
    auto *multi = helper->backend()->handle();
    Q_ASSERT(wlr_backend_is_multi(multi));
    wlr_backend *headlessHandle = nullptr;
    wlr_multi_for_each_backend(multi, [](wlr_backend *backend, void *data) {
        if (wlr_backend_is_headless(backend))
            *static_cast<wlr_backend **>(data) = backend;
    }, &headlessHandle);
    Q_ASSERT(headlessHandle);
    g_secondOutput = wlr_headless_add_output(headlessHandle, 1280, 720);
    Q_ASSERT(g_secondOutput);
    auto *woutput = WOutput::fromHandle(g_secondOutput);
    Q_ASSERT(woutput);
    wlr_output_create_global(g_secondOutput, woutput->server()->handle());

    QObject::connect(g_manager, &TreelandRegionWatchManagerInterfaceV1::regionWatchCreated,
                     [](RegionWatchV1 *watch) { g_watch = watch; });
}

extern "C" void region_watch_query_state(void *data)
{
    auto *state = static_cast<region_watch_server_state *>(data);
    *state = {};
    if (g_secondOutput)
        strncpy(state->second_output_name, g_secondOutput->name,
                sizeof(state->second_output_name) - 1);
    if (!g_watch)
        return;
    state->has_watch = 1;
    const QRect region = g_watch->region();
    state->has_region = !region.isEmpty();
    state->x = region.x();
    state->y = region.y();
    state->width = region.width();
    state->height = region.height();
}

extern "C" void region_watch_remove_second_output(void *)
{
    if (g_secondOutput) {
        wlr_output_destroy(g_secondOutput);
        g_secondOutput = nullptr;
    }
}

// Debug dump: all outputs known to the root container.
extern "C" void region_watch_dump_outputs(void *)
{
    const auto outputs = Helper::instance()->rootSurfaceContainer()->outputs();
    for (const auto &output : outputs) {
        auto *wOutput = output->output();
        qInfo() << "region-watch dump: output" << wOutput->name() << wOutput->size()
                << "scale" << wOutput->scale() << "pos" << wOutput->position();
    }
}

// Feeds an intersecting rect into the overlap check the same way the window
// tracker does; used to verify a watcher is inert after output_removed.
extern "C" void region_watch_trigger_recheck(void *)
{
    if (g_manager)
        g_manager->checkOverlapConflict({ QRect(0, 0, 1920, 1080) });
}
