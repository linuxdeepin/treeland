// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "desktop-integration-fixture.h"
#include "core/rootsurfacecontainer.h"
#include "protocol-test-server.h"
#include "seat/helper.h"
#include "core/shellhandler.h"
#include "surface/surfacewrapper.h"
#include "workspace/workspace.h"

#include <wbackend.h>

namespace {
desktop_fixture_state g_state {};
}

void protocol_test_desktop_setup(Helper *helper)
{
    protocol_test_create_headless_output(helper->backend(), false);
    QObject::connect(helper->shellHandler(), &ShellHandler::surfaceWrapperAdded, helper,
                     [helper](SurfaceWrapper *wrapper) {
                         g_state.wrapper_created = 1;
                         g_state.wrapper_in_workspace = helper->workspace()->surfaces().contains(wrapper) ? 1 : 0;
                     });
}

extern "C" void desktop_fixture_read_state(void *data)
{
    auto state = g_state;
    state.output_ready = !Helper::instance()->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
    *static_cast<desktop_fixture_state *>(data) = state;
}
