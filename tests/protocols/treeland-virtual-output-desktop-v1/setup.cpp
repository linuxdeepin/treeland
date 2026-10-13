// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "treeland-virtual-output-desktop-v1.h"

#include "treelandconfig.hpp"

#include <QObject>
#include <wbackend.h>
#include <woutput.h>

namespace {
Output *findRootOutputByName(WaylandServer *server, const QString &name)
{
    for (auto *output : server->rootSurfaceContainer()->outputs()) {
        if (output && output->output() && output->output()->name() == name)
            return output;
    }
    return nullptr;
}

virtual_output_desktop_state readState(WaylandServer *server)
{
    virtual_output_desktop_state state {};
    auto *first = findRootOutputByName(server, QStringLiteral("HEADLESS-1"));
    auto *second = findRootOutputByName(server, QStringLiteral("HEADLESS-2"));
    state.first_present = first ? 1 : 0;
    state.second_present = second ? 1 : 0;
    state.root_output_count = server->rootSurfaceContainer()->outputs().size();
    state.primary_is_first = server->rootSurfaceContainer()->primaryOutput() == first ? 1 : 0;
    state.first_is_source = first && first->isSource() ? 1 : 0;
    state.second_is_source = second && second->isSource() ? 1 : 0;
    state.second_is_copy = second && !second->isSource() ? 1 : 0;
    return state;
}

bool g_configNormalized = false;

void normalizeInitialOutputConfig(WaylandServer *server)
{
    auto *config = server->globalConfig();
    config->setCreateCopyOutput(false);
    config->setCopyOutputName(QString());
    config->setCopyOutputOutputs(QStringLiteral("[]"));
    server->setOutputMode(WaylandServer::OutputMode::Extension);
    g_configNormalized = true;
}
}

void protocol_test_setup(WaylandServer *server)
{
    // The desktop fixture starts with HEADLESS-1.  A second real backend
    // output is necessary because this protocol creates a copy group, not a
    // new backend output.
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
    return g_configNormalized
        && findRootOutputByName(server, QStringLiteral("HEADLESS-1"))
        && findRootOutputByName(server, QStringLiteral("HEADLESS-2"));
}

extern "C" void virtual_output_desktop_read_state(void *data)
{
    *static_cast<virtual_output_desktop_state *>(data) = readState(WaylandServer::instance());
}
