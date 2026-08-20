// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "wlr-output-management-unstable-v1.h"

#include <woutput.h>
#include <woutputhelper.h>
#include <woutputitem.h>
#include <woutputrenderwindow.h>
#include <woutputmanagerv1.h>

namespace {
int g_requestCount = 0;
int g_lastRequestTest = -1;
int g_lastRequestTransform = -1;
int g_lastRequestScaleMilli = -1;
}

void protocol_test_setup(Helper *helper)
{
    auto *manager = find_server_interface<WOutputManagerV1>(helper);
    Q_ASSERT(manager);
    QObject::connect(manager, &WOutputManagerV1::requestTestOrApply, helper,
                     [manager](wlr_output_configuration_v1 *config, bool onlyTest) {
                         const auto states = manager->stateListPending(config);
                         ++g_requestCount;
                         g_lastRequestTest = onlyTest;
                         if (states.isEmpty())
                             return;
                         g_lastRequestTransform = static_cast<int>(states.constFirst().transform);
                         g_lastRequestScaleMilli =
                             static_cast<int>(states.constFirst().scale * 1000.0f);
                     });
}

extern "C" void output_management_render(void *)
{
    const auto outputs = Helper::instance()->rootSurfaceContainer()->outputs();
    if (outputs.isEmpty())
        return;

    auto *viewport = outputs.constFirst()->screenViewport();
    // Force the production output frame that commits the queued transaction.
    Helper::instance()->window()->render(viewport, true);
}

extern "C" void output_management_read_server_state(void *data)
{
    output_management_server_state state {};
    const auto outputs = Helper::instance()->rootSurfaceContainer()->outputs();
    state.output_count = outputs.size();

    if (!outputs.isEmpty()) {
        auto *output = outputs.constFirst()->output();
        const auto *item = WOutputItem::getOutputItem(output);
        state.enabled = output->isEnabled();
        state.transform = static_cast<int>(output->orientation());
        state.scale_milli = static_cast<int>(output->scale() * 1000.0f);
        if (item) {
            state.x = static_cast<int>(item->x());
            state.y = static_cast<int>(item->y());
        }
        if (auto *helper = Helper::instance()->window()->getOutputHelper(outputs.constFirst()->screenViewport())) {
            const auto pending = helper->extraState();
            state.pending_state = pending ? 1 : 0;
            if (pending) {
                state.pending_transform = static_cast<int>(pending->transform);
                state.pending_scale_milli = static_cast<int>(pending->scale * 1000.0f);
            }
        }
    }
    state.request_count = g_requestCount;
    state.last_request_test = g_lastRequestTest;
    state.last_request_transform = g_lastRequestTransform;
    state.last_request_scale_milli = g_lastRequestScaleMilli;

    *static_cast<output_management_server_state *>(data) = state;
}
