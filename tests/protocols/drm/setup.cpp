// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "drm.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "workspace/workspace.h"

#include <QEventLoop>
#include <QFutureWatcher>
#include <QImage>

#include <wbackend.h>
#include <woutputrenderwindow.h>
#include <wsurfaceitem.h>
#include <wtextureproviderprovider.h>

namespace {
SurfaceWrapper *g_wrapper = nullptr;
drm_render_state g_state {};
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
                         g_state.wrapper_created = 1;
                         g_state.wrapper_in_workspace = helper->workspace()->surfaces().contains(wrapper) ? 1 : 0;
                     });
}

extern "C" void drm_read_render_state(void *data)
{
    auto state = g_state;
    state.output_ready = !Helper::instance()->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
    if (!g_wrapper || !g_wrapper->surfaceItem()) {
        *static_cast<drm_render_state *>(data) = state;
        return;
    }

    auto *content = g_wrapper->surfaceItem()->findItemContent();
    if (!content) {
        *static_cast<drm_render_state *>(data) = state;
        return;
    }
    auto *renderWindow = content->outputRenderWindow();
    if (!renderWindow) {
        *static_cast<drm_render_state *>(data) = state;
        return;
    }

    renderWindow->render();
    WTextureCapturer capturer(content);
    QFutureWatcher<QImage> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<QImage>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(capturer.grabToImage());
    loop.exec();
    if (watcher.future().isCanceled()) {
        *static_cast<drm_render_state *>(data) = state;
        return;
    }

    const QImage image = watcher.result();
    if (image.isNull()) {
        *static_cast<drm_render_state *>(data) = state;
        return;
    }
    const QColor sample = image.pixelColor(image.width() / 2, image.height() / 2);
    state.image_ready = 1;
    state.image_width = image.width();
    state.image_height = image.height();
    state.sample_red = sample.red();
    state.sample_green = sample.green();
    state.sample_blue = sample.blue();
    state.sample_alpha = sample.alpha();
    *static_cast<drm_render_state *>(data) = state;
}
