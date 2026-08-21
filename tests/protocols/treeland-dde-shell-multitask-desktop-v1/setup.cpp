// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/rootsurfacecontainer.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "treeland-dde-shell-multitask-desktop-v1.h"
#include "workspace/workspace.h"

#include <QQuickItem>
#include <QtMath>
#include <wbackend.h>

namespace {
QQuickItem *multitaskItem(Helper *helper)
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
    if (auto *item = findInScene(findInScene, helper->rootSurfaceContainer())) {
        return item;
    }
    return nullptr;
}
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
}

extern "C" void dde_multitask_desktop_read_state(void *data)
{
    dde_multitask_desktop_state state {};
    auto *helper = Helper::instance();
    auto *item = multitaskItem(helper);
    state.output_ready = !helper->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
    state.workspace_window_count = helper->workspace()->surfaces().size();
    state.multitask_created = item ? 1 : 0;
    state.mode_is_multitask = helper->currentMode() == Helper::CurrentMode::Multitaskview ? 1 : 0;
    state.mode_is_normal = helper->currentMode() == Helper::CurrentMode::Normal ? 1 : 0;
    if (item) {
        state.multitask_status = item->property("status").toInt();
        state.active_reason = item->property("activeReason").toInt();
        state.partial_factor_milli = qRound(item->property("partialFactor").toReal() * 1000.0);
    }
    *static_cast<dde_multitask_desktop_state *>(data) = state;
}
