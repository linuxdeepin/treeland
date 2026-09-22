// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/shellhandler.h"
#include "seat/helper.h"
#include "server-bridge.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>

namespace {
SurfaceWrapper *g_toplevel = nullptr;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
    QObject::connect(helper->shellHandler(), &ShellHandler::surfaceWrapperAdded, helper,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel)
                             g_toplevel = wrapper;
                     });
}

extern "C" void keyboard_shortcuts_inhibit_focus(void *)
{
    if (g_toplevel)
        Helper::instance()->activateSurface(g_toplevel, Qt::ActiveWindowFocusReason);
}
