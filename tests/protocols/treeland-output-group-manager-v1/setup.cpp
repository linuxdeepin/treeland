// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/rootsurfacecontainer.h"
#include "core/waylandserver.h"
#include "modules/output-group/outputgroupmanagerinterfacev1.h"
#include "output/output.h"
#include "server-bridge.h"
#include "treeland-output-group-manager-v1.h"
#include "treelandconfig.hpp"

#include <QObject>

#include <cstdio>
#include <cstring>

#include <wbackend.h>
#include <woutput.h>
#include <woutputrenderwindow.h>
#include <wscopedvalue.h>
#include <wlr_all.h>

namespace {
void appendSpaceSeparated(char *target, size_t size, const QString &value)
{
    const QByteArray raw = value.toUtf8();
    const size_t used = strlen(target);
    if (used + raw.size() + 2 > size)
        return;
    if (used)
        target[used] = ' ';
    memcpy(target + used + (used ? 1 : 0), raw.constData(), raw.size());
    target[used + (used ? 1 : 0) + raw.size()] = '\0';
}

output_group_server_state readState(WaylandServer *server)
{
    output_group_server_state state {};
    const auto &outputs = server->rootSurfaceContainer()->outputs();
    state.output_count = outputs.size();
    if (state.output_count > OUTPUT_GROUP_TEST_MAX_OUTPUTS)
        state.output_count = OUTPUT_GROUP_TEST_MAX_OUTPUTS;

    for (int i = 0; i < state.output_count; ++i) {
        Output *output = outputs.at(i);
        state.proxy_index[i] = -1;
        if (!output || !output->output())
            continue;
        snprintf(state.uuids[i], OUTPUT_GROUP_TEST_MAX_NAME, "%s", output->uuid().toUtf8().constData());
        snprintf(state.names[i], OUTPUT_GROUP_TEST_MAX_NAME, "%s", output->output()->name().toUtf8().constData());
        state.is_enabled[i] = output->output()->isEnabled() ? 1 : 0;
        state.is_source[i] = output->isSource() ? 1 : 0;
        state.is_copy[i] = output->isSource() ? 0 : 1;
        if (state.is_copy[i]) {
            for (int j = 0; j < state.output_count; ++j) {
                if (outputs.at(j) == output->proxyOutput())
                    state.proxy_index[i] = j;
            }
        }
    }

    if (auto *manager = find_server_interface<OutputGroupManagerInterfaceV1>(server)) {
        const auto groups = manager->effectiveGroups();
        state.group_count = groups.size();
        for (const auto &group : groups) {
            appendSpaceSeparated(state.group_names, sizeof(state.group_names), group.name);
            appendSpaceSeparated(state.group_sources, sizeof(state.group_sources), group.sourceUuid);
        }
    }
    return state;
}

bool g_configNormalized = false;

void normalizeInitialOutputConfig(WaylandServer *server)
{
    // The protocol test asserts the bind-time group dump is empty, so drop any
    // group persisted by an earlier run.
    server->globalConfig()->setOutputGroups(QStringLiteral("[]"));
    g_configNormalized = true;
}

// The harness starts the headless outputs disabled. The protocol only accepts
// connected *and enabled* outputs, so the fixture turns them on like a user
// session would.
void ensureOutputsEnabled(WaylandServer *server)
{
    for (auto *output : server->rootSurfaceContainer()->outputs()) {
        if (output && output->output() && !output->output()->isEnabled())
            output->enable();
    }
}
} // namespace

void protocol_test_setup(WaylandServer *server)
{
    // Copy mode mirrors existing outputs, so the fixture needs four real
    // backend outputs: two groups with a source and a mirror each. The harness
    // already provides one output.
    for (int i = 0; i < OUTPUT_GROUP_TEST_MAX_OUTPUTS - 1; ++i)
        add_headless_output(server->backend(), false);

    auto *config = server->globalConfig();
    if (config->isInitializeSucceeded()) {
        normalizeInitialOutputConfig(server);
    } else {
        QObject::connect(config,
                         &TreelandConfig::configInitializeSucceed,
                         server,
                         [server](auto *) { normalizeInitialOutputConfig(server); });
    }
}

extern "C" bool protocol_test_ready(WaylandServer *server)
{
    ensureOutputsEnabled(server);
    return g_configNormalized
        && server->rootSurfaceContainer()->outputs().size() == OUTPUT_GROUP_TEST_MAX_OUTPUTS;
}

// The fixture can enable or disable an output by uuid, like an unplug or a
// user turning a screen off. Disabling is followed by an availability
// notification so the compositor re-evaluates the output groups.
extern "C" void output_group_set_output_enabled(void *data)
{
    auto *request = static_cast<output_group_output_state *>(data);
    auto *server = WaylandServer::instance();
    const QString uuid = QString::fromUtf8(request->uuid);

    for (auto *output : server->rootSurfaceContainer()->outputs()) {
        if (!output || !output->output() || output->uuid() != uuid)
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

    if (auto *manager = find_server_interface<OutputGroupManagerInterfaceV1>(server))
        manager->notifyOutputAvailabilityChanged();
}

extern "C" void output_group_render(void *data)
{
    Q_UNUSED(data);
    const auto &outputs = WaylandServer::instance()->rootSurfaceContainer()->outputs();

    // Force the production output frames that commit a queued output-management
    // transaction, so a wlr-output-management apply can complete in the test.
    // Every output is rendered: which one carries the commit job depends on the
    // order the outputs were registered in.
    for (auto *output : outputs) {
        if (output && output->screenViewport())
            WaylandServer::instance()->window()->render(output->screenViewport(), true);
    }
}

extern "C" void output_group_read_state(void *data)
{
    *static_cast<output_group_server_state *>(data) = readState(WaylandServer::instance());
}
