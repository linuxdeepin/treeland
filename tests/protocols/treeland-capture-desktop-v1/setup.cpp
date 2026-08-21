// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "modules/capture/capture.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "treeland-capture-desktop-v1.h"
#include "workspace/workspace.h"

#include <wbackend.h>
#include <woutputrenderwindow.h>
#include <wsurfaceitem.h>

namespace {
SurfaceWrapper *g_wrapper = nullptr;
capture_desktop_selection_state g_state {};

void copyState(void *data)
{
    *static_cast<capture_desktop_selection_state *>(data) = g_state;
}
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
    QObject::connect(helper->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     helper,
                     [helper](SurfaceWrapper *wrapper) {
                         if (wrapper->type() != SurfaceWrapper::Type::XdgToplevel)
                             return;
                         g_wrapper = wrapper;
                         g_state.wrapper_ready = 1;
                         g_state.output_ready = !helper->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
                     });
}

extern "C" void capture_desktop_select_mapped_surface(void *data)
{
    auto *helper = Helper::instance();
    g_state.output_ready = !helper->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
    if (!g_wrapper || !helper->workspace()->surfaces().contains(g_wrapper)
        || !g_wrapper->surfaceItem()) {
        copyState(data);
        return;
    }

    auto *content = g_wrapper->surfaceItem()->findItemContent();
    if (!content) {
        copyState(data);
        return;
    }
    g_state.surface_content_ready = 1;
    const auto paintOrder = WOutputRenderWindow::paintOrderItemList(
        helper->window()->contentItem(), [](QQuickItem *) { return true; });
    g_state.content_in_paint_order = paintOrder.contains(content) ? 1 : 0;
    g_state.content_visible = content->isVisible() ? 1 : 0;
    g_state.content_width = qRound(content->width());
    g_state.content_height = qRound(content->height());

    // Helper creates this production QML selector in response to the client's
    // select_source request.  The test never calls CaptureContextV1::setSource().
    auto *selector = helper->rootSurfaceContainer()->findChild<CaptureSourceSelector *>();
    if (!selector) {
        copyState(data);
        return;
    }
    g_state.selector_ready = 1;

    // Select the verified mapped production surface through the selector's
    // production selection entry point.
    helper->window()->render();
    selector->selectSurface(content);
    g_state.hovered_mapped_content = 1;

    auto *context = selector->captureManager()->contextInSelection();
    auto *source = context ? context->source() : nullptr;
    g_state.source_selected = source ? 1 : 0;
    g_state.source_is_surface = source && source->sourceType() == CaptureSource::Surface ? 1 : 0;
    if (source) {
        const QSize sourceSize = source->sourceSize();
        g_state.source_width = sourceSize.width();
        g_state.source_height = sourceSize.height();
    }
    copyState(data);
}

extern "C" void capture_desktop_render_selected_source(void *data)
{
    auto *helper = Helper::instance();
    helper->window()->render();
    g_state.render_requested = 1;
    copyState(data);
}
