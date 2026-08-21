// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "core/windowpicker.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "treeland-dde-shell-picker-desktop-v1.h"
#include "workspace/workspace.h"

#include <wbackend.h>
#include <wsurfaceitem.h>

namespace {
SurfaceWrapper *g_wrapper = nullptr;
dde_picker_desktop_state g_state {};

QQuickItem *findWindowPicker(QQuickItem *item)
{
    if (item->property("hint").toString() == QStringLiteral("protocol picker"))
        return item;
    for (auto *child : item->childItems()) {
        if (auto *picker = findWindowPicker(child))
            return picker;
    }
    return nullptr;
}

QQuickItem *findWindowPicker(QObject *object, int *objectCount)
{
    ++*objectCount;
    if (auto *item = qobject_cast<QQuickItem *>(object);
        item && item->property("hint").toString() == QStringLiteral("protocol picker"))
        return item;

    for (auto *child : object->children()) {
        if (auto *picker = findWindowPicker(child, objectCount))
            return picker;
    }
    return nullptr;
}
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
    auto *manager = helper->backend()->server()->findInterface<DDEShellManagerInterfaceV1>();
    g_state.manager_found = manager ? 1 : 0;
    if (manager) {
        QObject::connect(manager,
                         &DDEShellManagerInterfaceV1::PickerCreated,
                         helper,
                         [](WindowPickerInterface *) { g_state.picker_resource_created = 1; });
        QObject::connect(manager,
                         &DDEShellManagerInterfaceV1::requestPickWindow,
                         helper,
                         [](WindowPickerInterface *) { g_state.pick_request_received = 1; });
    }
    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [helper](SurfaceWrapper *wrapper) {
                         if (wrapper->type() != SurfaceWrapper::Type::XdgToplevel)
                             return;
                         g_wrapper = wrapper;
                         g_state.wrapper_ready = 1;
                         g_state.wrapper_in_workspace =
                             helper->workspace()->surfaces().contains(wrapper) ? 1 : 0;
                     });
}

extern "C" void dde_picker_desktop_select_mapped_window(void *data)
{
    auto *helper = Helper::instance();
    g_state.output_ready = !helper->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
    if (!g_wrapper || !helper->workspace()->surfaces().contains(g_wrapper)
        || !g_wrapper->surfaceItem()) {
        *static_cast<dde_picker_desktop_state *>(data) = g_state;
        return;
    }

    auto *root = helper->rootSurfaceContainer();
    const auto windowPickers = root->findChildren<WindowPicker *>();
    g_state.window_picker_instances = windowPickers.size();
    QQuickItem *picker = windowPickers.isEmpty() ? nullptr : windowPickers.constLast();
    if (!picker)
        picker = findWindowPicker(root);
    if (!picker)
        picker = findWindowPicker(root, &g_state.root_object_count);
    auto *windowPicker = qobject_cast<WindowPicker *>(picker);
    if (!windowPicker) {
        *static_cast<dde_picker_desktop_state *>(data) = g_state;
        return;
    }
    g_state.picker_created = 1;
    windowPicker->selectWindow(g_wrapper->surfaceItem());
    g_state.mapped_window_selected = 1;
    *static_cast<dde_picker_desktop_state *>(data) = g_state;
}
