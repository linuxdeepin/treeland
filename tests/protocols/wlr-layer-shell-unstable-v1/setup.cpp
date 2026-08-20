// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/shellhandler.h"
#include "core/rootsurfacecontainer.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "wlr-layer-shell-unstable-v1.h"
#include <wlayersurface.h>
namespace { SurfaceWrapper *g_wrapper = nullptr; }
void protocol_test_setup(Helper *helper) {
    add_headless_output(helper->backend(), false);
    QObject::connect(helper->shellHandler(), &ShellHandler::surfaceWrapperAdded, helper,
        [](SurfaceWrapper *wrapper) { if (wrapper->type() == SurfaceWrapper::Type::Layer) g_wrapper = wrapper; });
}
extern "C" void layer_shell_read_state(void *data) {
    layer_shell_server_state state {};
    auto *layer = g_wrapper ? qobject_cast<WLayerSurface *>(g_wrapper->shellSurface()) : nullptr;
    state.wrapper = layer != nullptr;
    state.container = g_wrapper && g_wrapper->container();
    if (layer) { state.layer = (int)layer->layer(); state.anchor = layer->ancher().toInt(); state.exclusive_zone = layer->exclusiveZone(); state.top_margin = layer->topMargin(); state.keyboard_exclusive = layer->keyboardInteractivity() == WLayerSurface::KeyboardInteractivity::Exclusive; }
    auto *seat = Helper::instance()->rootSurfaceContainer()->getSeatContainerOrDefault();
    state.focused = seat && seat->keyboardFocusSurface() == g_wrapper;
    *static_cast<layer_shell_server_state *>(data) = state;
}
