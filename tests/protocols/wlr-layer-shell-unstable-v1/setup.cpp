// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/layersurfacecontainer.h"
#include "core/shellhandler.h"
#include "core/rootsurfacecontainer.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "output/output.h"
#include "surface/surfacewrapper.h"
#include "wlr-layer-shell-unstable-v1.h"
#include <wlayersurface.h>
#include <woutput.h>
#include <wlr_all.h>
namespace {
SurfaceWrapper *g_wrapper = nullptr;
SurfaceWrapper *g_window = nullptr;
}
void protocol_test_setup(WaylandServer *server) {
    add_headless_output(server->backend(), false);
    QObject::connect(server->shellHandler(), &ShellHandler::surfaceWrapperAdded, server,
        [](SurfaceWrapper *wrapper) {
            if (wrapper->type() == SurfaceWrapper::Type::Layer)
                g_wrapper = wrapper;
            else
                g_window = wrapper;
        });
}
extern "C" void layer_shell_read_state(void *data) {
    layer_shell_server_state state {};
    auto *layer = g_wrapper ? qobject_cast<WLayerSurface *>(g_wrapper->shellSurface()) : nullptr;
    state.wrapper = layer != nullptr;
    state.container = g_wrapper && g_wrapper->container();
    if (layer) { state.layer = (int)layer->layer(); state.anchor = layer->ancher().toInt(); state.exclusive_zone = layer->exclusiveZone(); state.top_margin = layer->topMargin(); state.keyboard_exclusive = layer->keyboardInteractivity() == WLayerSurface::KeyboardInteractivity::Exclusive; }
    auto *root = WaylandServer::instance()->rootSurfaceContainer();
    // The test relies on the layer surface living on the primary output, which is
    // the output the removal below destroys.
    auto *container = qobject_cast<OutputLayerSurfaceContainer *>(g_wrapper ? g_wrapper->container() : nullptr);
    state.on_primary = container && container->output() == root->primaryOutput();
    auto *seat = root->getSeatContainerOrDefault();
    state.focused = seat && seat->keyboardFocusSurface() == g_wrapper;
    *static_cast<layer_shell_server_state *>(data) = state;
}
// Destroys the primary output from the compositor thread. wlr_output_destroy()
// drives the same WBackend::outputRemoved() -> OutputManager::outputRemoved()
// chain as a DRM unplug or a VT switch: every output's layer surfaces are torn
// down, layer surfaces created without an output are re-homed to the primary
// output (which must never be the output being removed), and the windows of the
// removed output have to follow the new primary.
extern "C" void layer_shell_remove_primary_output(void *data) {
    layer_shell_output_removal_state state {};
    auto *root = WaylandServer::instance()->rootSurfaceContainer();
    state.primary_before = root->primaryOutput() != nullptr;
    state.outputs_before = root->outputs().size();
    for (auto *o : root->outputs()) {
        if (o != root->primaryOutput() || !o->output())
            continue;
        auto *handle = o->output()->handle();
        wlr_output_destroy(handle);
        break;
    }
    state.primary_after = root->primaryOutput() != nullptr;
    state.outputs_after = root->outputs().size();
    *static_cast<layer_shell_output_removal_state *>(data) = state;
}
extern "C" void layer_shell_read_window_state(void *data) {
    layer_shell_window_state state {};
    auto *root = WaylandServer::instance()->rootSurfaceContainer();
    state.mapped = g_window && g_window->shellSurface();
    if (g_window) {
        auto *owns = g_window->ownsOutput();
        state.owns_output = owns != nullptr;
        state.owns_primary = owns && owns == root->primaryOutput();
        state.inside_own_output = owns && owns->geometry().contains(g_window->geometry().center());
    }
    *static_cast<layer_shell_window_state *>(data) = state;
}
