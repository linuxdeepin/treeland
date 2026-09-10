// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/layersurfacecontainer.h"
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "output/output.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "treeland-virtual-output-desktop-v1.h"

#include "treelandconfig.hpp"

#include <QObject>
#include <QPointer>
#include <wbackend.h>
#include <wlayersurface.h>
#include <woutput.h>
#include <woutputrenderwindow.h>

namespace {
Output *findRootOutputByName(Helper *helper, const QString &name)
{
    for (auto *output : helper->rootSurfaceContainer()->outputs()) {
        if (output && output->output() && output->output()->name() == name)
            return output;
    }
    return nullptr;
}

// The layer-shell surface the client binds to HEADLESS-2. Kept as a QPointer:
// in the regression this test guards, the copy<->normal output wrapper swap
// wrongly closed the surface, which would destroy this wrapper.
QPointer<SurfaceWrapper> g_layerWrapper;

virtual_output_desktop_state readState(Helper *helper)
{
    virtual_output_desktop_state state {};
    auto *first = findRootOutputByName(helper, QStringLiteral("HEADLESS-1"));
    auto *second = findRootOutputByName(helper, QStringLiteral("HEADLESS-2"));
    state.first_present = first ? 1 : 0;
    state.second_present = second ? 1 : 0;
    state.root_output_count = helper->rootSurfaceContainer()->outputs().size();
    state.primary_is_first = helper->rootSurfaceContainer()->primaryOutput() == first ? 1 : 0;
    state.first_is_source = first && first->isSource() ? 1 : 0;
    state.second_is_source = second && second->isSource() ? 1 : 0;
    state.second_is_copy = second && !second->isSource() ? 1 : 0;
    state.first_disabled = first && first->output() && !first->output()->isEnabled() ? 1 : 0;
    state.layer_alive = g_layerWrapper != nullptr;
    state.layer_on_second = g_layerWrapper && g_layerWrapper->ownsOutput() == second ? 1 : 0;
    state.layer_container_on_second =
        g_layerWrapper && g_layerWrapper->container()
        && qobject_cast<OutputLayerSurfaceContainer *>(g_layerWrapper->container())->output()
            == second
        ? 1
        : 0;
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

    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::Layer)
                             g_layerWrapper = wrapper;
                     });
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

extern "C" void virtual_output_desktop_render(void *)
{
    // Force the production output frames that commit the queued disable
    // transaction so the configuration result event is emitted.
    for (auto *output : Helper::instance()->rootSurfaceContainer()->outputs()) {
        if (auto *viewport = output->screenViewport())
            Helper::instance()->window()->render(viewport, true);
    }
}