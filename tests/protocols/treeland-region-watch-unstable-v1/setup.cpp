// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/region-watch/regionwatchmanagerinterfacev1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "treeland-region-watch-unstable-v1.h"

#include "core/rootsurfacecontainer.h"
#include "output/output.h"

#include <wbackend.h>
#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
QPointer<RegionWatchV1> g_watch;
TreelandRegionWatchManagerInterfaceV1 *g_manager = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    g_manager = find_server_interface<TreelandRegionWatchManagerInterfaceV1>(helper);
    Q_ASSERT(g_manager);

    // TODO(multi-output): the region-watch protocol also defines output
    // removal semantics (output_removed + watcher becoming inert) and
    // multi-output coordinate translation. Covering those requires a second
    // headless output that is wired into the output layout so it has a real
    // WOutput wrapper and a layout position. Until that fixture path is
    // available, only the single-output state machine is tested.

    QObject::connect(g_manager, &TreelandRegionWatchManagerInterfaceV1::regionWatchCreated,
                     [](RegionWatchV1 *watch) { g_watch = watch; });
}

extern "C" void region_watch_query_state(void *data)
{
    auto *state = static_cast<region_watch_server_state *>(data);
    *state = {};
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

// Runs the exact production evaluation the 300 ms debounce timer runs, but
// synchronously on the compositor thread: invoke_on_server_thread returning
// is the completion boundary for enter/leave assertions (the framework's
// sync rules forbid fixed delays and retry polling).
extern "C" void region_watch_recheck(void *)
{
    if (g_manager)
        g_manager->recheck();
}

// Layout position of the named output, from the same WOutput::position() the
// region translation uses.
extern "C" void region_watch_output_position(void *data)
{
    auto *query = static_cast<region_watch_output_pos *>(data);
    query->found = 0;
    const auto outputs = Helper::instance()->rootSurfaceContainer()->outputs();
    for (const auto &output : outputs) {
        auto *wOutput = output->output();
        if (wOutput->name() == QLatin1String(query->name)) {
            const QPoint pos = wOutput->position();
            query->x = pos.x();
            query->y = pos.y();
            query->found = 1;
            return;
        }
    }
}
