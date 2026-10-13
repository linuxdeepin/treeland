// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/rootsurfacecontainer.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "treeland-dde-shell-multitask-desktop-v1.h"
#include "workspace/workspace.h"

#include <QQuickItem>
#include <QtMath>
#include <wbackend.h>

namespace {
QQuickItem *multitaskItem(WaylandServer *server)
{
    const auto findInScene = [](auto &&self, QQuickItem *item) -> QQuickItem * {
        // QML may wrap the C++ item in a generated meta-object name. Its
        // public Multitaskview properties are the stable production identity.
        if (item->property("status").isValid() && item->property("activeReason").isValid()
            && item->property("partialFactor").isValid())
            return item;
        for (auto *child : item->childItems()) {
            if (auto *found = self(self, child))
                return found;
        }
        return nullptr;
    };
    if (auto *item = findInScene(findInScene, server->rootSurfaceContainer())) {
        return item;
    }
    return nullptr;
}
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);
}

extern "C" void dde_multitask_desktop_read_state(void *data)
{
    dde_multitask_desktop_state state {};
    auto *server = WaylandServer::instance();
    auto *item = multitaskItem(server);
    state.output_ready = !server->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
    state.workspace_window_count = server->workspace()->surfaces().size();
    state.multitask_created = item ? 1 : 0;
    state.mode_is_multitask = server->currentMode() == WaylandServer::CurrentMode::Multitaskview ? 1 : 0;
    state.mode_is_normal = server->currentMode() == WaylandServer::CurrentMode::Normal ? 1 : 0;
    if (item) {
        state.multitask_status = item->property("status").toInt();
        state.active_reason = item->property("activeReason").toInt();
        state.partial_factor_milli = qRound(item->property("partialFactor").toReal() * 1000.0);
    }
    *static_cast<dde_multitask_desktop_state *>(data) = state;
}
