// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "protocol-test-server.h"
#include "rendered-output-fixture.h"
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
rendered_output_fixture_state g_state {};
}

void protocol_test_desktop_setup(Helper *helper)
{
    protocol_test_create_headless_output(helper->backend(), false);
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

extern "C" void rendered_output_fixture_read_state(void *data)
{
    auto state = g_state;
    state.output_ready = !Helper::instance()->rootSurfaceContainer()->outputs().isEmpty() ? 1 : 0;
    if (!g_wrapper || !g_wrapper->surfaceItem()) {
        *static_cast<rendered_output_fixture_state *>(data) = state;
        return;
    }

    // SurfaceWrapper installs Treeland's SurfaceContent QML delegate.  The
    // WSurfaceItem contentItem() is consequently the delegate's outer
    // container, while WSurfaceItemContent is a child in that real delegate
    // tree.  Use Waylib's production lookup instead of assuming no delegate.
    auto *content = g_wrapper->surfaceItem()->findItemContent();
    if (!content) {
        *static_cast<rendered_output_fixture_state *>(data) = state;
        return;
    }
    state.surface_content_ready = 1;
    auto *renderWindow = content->outputRenderWindow();
    if (!renderWindow) {
        *static_cast<rendered_output_fixture_state *>(data) = state;
        return;
    }
    state.render_window_ready = 1;

    renderWindow->render();
    WTextureCapturer capturer(content);
    QFutureWatcher<QImage> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<QImage>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(capturer.grabToImage());
    loop.exec();
    if (watcher.future().isCanceled()) {
        *static_cast<rendered_output_fixture_state *>(data) = state;
        return;
    }

    const QImage image = watcher.result();
    if (image.isNull()) {
        *static_cast<rendered_output_fixture_state *>(data) = state;
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
    *static_cast<rendered_output_fixture_state *>(data) = state;
}
