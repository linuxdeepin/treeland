// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "screencopy-test.h"
#include "server-bridge.h"
#include "seat/helper.h"

#include <woutput.h>
#include <woutputrenderwindow.h>

#include <wlr_all.h>

void protocol_test_setup(Helper *helper)
{
    Q_ASSERT(add_headless_output(helper->backend(), false, 1920, 1080));
}

extern "C" void screencopy_render(void *data)
{
    auto *state = static_cast<screencopy_render_state *>(data);
    const auto outputs = Helper::instance()->rootSurfaceContainer()->outputs();
    state->output_count = outputs.size();
    if (outputs.isEmpty())
        return;

    auto *output = outputs.constFirst();
    auto *woutput = output->output();
    auto *handle = woutput->handle();
    auto *window = Helper::instance()->window();
    state->output_enabled_before = handle->enabled;
    state->needs_frame_before = handle->needs_frame;
    state->frame_pending_before = handle->frame_pending;
    state->attach_render_locks_before = handle->attach_render_locks;

    const auto connection = QObject::connect(
        window, &WOutputRenderWindow::renderEnd, window,
        [state, woutput](const QList<QPointer<WOutput>> &committedOutputs) {
            state->render_end_count++;
            state->target_committed = committedOutputs.contains(woutput);
        });
    window->render(output->screenViewport(), true);
    QObject::disconnect(connection);

    state->output_enabled_after = handle->enabled;
    state->needs_frame_after = handle->needs_frame;
    state->frame_pending_after = handle->frame_pending;
    state->attach_render_locks_after = handle->attach_render_locks;
}
