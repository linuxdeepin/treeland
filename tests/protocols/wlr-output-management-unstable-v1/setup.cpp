// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "wlr-output-management-unstable-v1.h"

#include <woutput.h>
#include <woutputhelper.h>
#include <woutputitem.h>
#include <woutputrenderwindow.h>
#include <woutputmanagerv1.h>
#include <wscopedvalue.h>
#include <wlr_all.h>

#include <cstdio>

namespace {
int g_requestCount = 0;
int g_lastRequestTest = -1;
int g_lastRequestTransform = -1;
int g_lastRequestScaleMilli = -1;
}

void protocol_test_setup(WaylandServer *server)
{
    // An apply that touches several heads only has a code path with more than
    // one: the harness already provides one output, the fixture adds a second.
    // Without the second output protocol_test_ready() never becomes true, so a
    // failure here has to abort instead of turning into a test timeout.
    if (!add_headless_output(server->backend(), false))
        qFatal("the multi-head apply cases need a second headless output");

    auto *manager = find_server_interface<WOutputManagerV1>(server);
    Q_ASSERT(manager);
    QObject::connect(manager, &WOutputManagerV1::requestTestOrApply, server,
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

extern "C" bool protocol_test_ready(WaylandServer *server)
{
    return server->rootSurfaceContainer()->outputs().size() >= 2;
}

// Turns one output off (or back on) behind the protocol's back, the way a DPMS
// timeout or a display panel switch does.
extern "C" void output_management_set_output_enabled(void *data)
{
    auto *request = static_cast<output_management_output_request *>(data);
    const QString name = QString::fromUtf8(request->name);

    for (auto *output : WaylandServer::instance()->rootSurfaceContainer()->outputs()) {
        if (!output || !output->output() || output->output()->name() != name)
            continue;
        if (request->enabled == output->output()->isEnabled())
            break;

        if (request->enabled) {
            output->enable();
        } else {
            WOutputStateGuard state;
            wlr_output_state_set_enabled(state.get(), false);
            if (!wlr_output_commit_state(output->output()->handle(), state.get()))
                return;
        }
        break;
    }
}

// Forces a frame on every output: the production output frames commit the
// queued output-management transactions, and which output carries a commit job
// depends on the configuration that was applied.
extern "C" void output_management_render(void *)
{
    const auto outputs = WaylandServer::instance()->rootSurfaceContainer()->outputs();
    for (auto *output : outputs) {
        if (output && output->screenViewport())
            WaylandServer::instance()->window()->render(output->screenViewport(), true);
    }
}

extern "C" void output_management_read_server_state(void *data)
{
    auto *state = static_cast<output_management_server_state *>(data);
    const auto outputs = WaylandServer::instance()->rootSurfaceContainer()->outputs();
    state->output_count = outputs.size();
    if (state->output_count > OUTPUT_MANAGEMENT_MAX_OUTPUTS)
        state->output_count = OUTPUT_MANAGEMENT_MAX_OUTPUTS;

    for (int i = 0; i < state->output_count; ++i) {
        Output *output = outputs.at(i);
        auto &entry = state->outputs[i];
        entry = {};
        if (!output || !output->output())
            continue;

        snprintf(entry.name, sizeof(entry.name), "%s", output->output()->name().toUtf8().constData());
        entry.enabled = output->output()->isEnabled();
        entry.transform = static_cast<int>(output->output()->orientation());
        entry.scale_milli = static_cast<int>(output->output()->scale() * 1000.0f);
        if (const auto *item = WOutputItem::getOutputItem(output->output())) {
            entry.x = static_cast<int>(item->x());
            entry.y = static_cast<int>(item->y());
        }
        if (auto *outputHelper =
                WaylandServer::instance()->window()->getOutputHelper(output->screenViewport())) {
            const auto pending = outputHelper->extraState();
            entry.pending_state = pending ? 1 : 0;
            if (pending) {
                entry.pending_transform = static_cast<int>(pending->transform);
                entry.pending_scale_milli = static_cast<int>(pending->scale * 1000.0f);
            }
        }
    }

    state->request_count = g_requestCount;
    state->last_request_test = g_lastRequestTest;
    state->last_request_transform = g_lastRequestTransform;
    state->last_request_scale_milli = g_lastRequestScaleMilli;
}
