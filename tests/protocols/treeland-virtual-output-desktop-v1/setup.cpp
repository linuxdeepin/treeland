// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "treeland-virtual-output-desktop-v1.h"

#include "treelandconfig.hpp"

#include <QObject>
#include <wbackend.h>
#include <woutput.h>

namespace {
Output *findRootOutputByName(Helper *helper, const QString &name)
{
    for (auto *output : helper->rootSurfaceContainer()->outputs()) {
        if (output && output->output() && output->output()->name() == name)
            return output;
    }
    return nullptr;
}

virtual_output_desktop_state readState(Helper *helper)
{
    virtual_output_desktop_state state {};
    auto *first = findRootOutputByName(helper, QStringLiteral("HEADLESS-1"));
    auto *second = findRootOutputByName(helper, QStringLiteral("HEADLESS-2"));
    state.first_present = first ? 1 : 0;
    state.second_present = second ? 1 : 0;
    state.root_output_count = helper->rootSurfaceContainer()->outputs().size();
    state.primary_is_first = helper->rootSurfaceContainer()->primaryOutput() == first ? 1 : 0;
    state.first_is_normal = first && first->isPrimary() ? 1 : 0;
    state.second_is_normal = second && second->isPrimary() ? 1 : 0;
    state.second_is_copy = second && !second->isPrimary() ? 1 : 0;
    return state;
}

bool g_configNormalized = false;

void normalizeInitialOutputConfig(Helper *helper)
{
    auto *config = helper->globalConfig();
    config->setCreateCopyOutput(false);
    config->setCopyOutputName(QString());
    config->setCopyOutputOutputs(QStringLiteral("[]"));
    helper->setOutputMode(Helper::OutputMode::Extension);
    g_configNormalized = true;
}
}

void protocol_test_setup(Helper *helper)
{
    // The desktop fixture starts with HEADLESS-1.  A second real backend
    // output is necessary because this protocol creates a copy group, not a
    // new backend output.
    add_headless_output(helper->backend(), false);

    auto *config = helper->globalConfig();
    if (config->isInitializeSucceeded()) {
        normalizeInitialOutputConfig(helper);
    } else {
        QObject::connect(config,
                         &TreelandConfig::configInitializeSucceed,
                         helper,
                         [helper](auto *) { normalizeInitialOutputConfig(helper); });
    }
}

extern "C" bool protocol_test_ready(Helper *helper)
{
    return g_configNormalized
        && findRootOutputByName(helper, QStringLiteral("HEADLESS-1"))
        && findRootOutputByName(helper, QStringLiteral("HEADLESS-2"));
}

extern "C" void virtual_output_desktop_read_state(void *data)
{
    *static_cast<virtual_output_desktop_state *>(data) = readState(Helper::instance());
}
