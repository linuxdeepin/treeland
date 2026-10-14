// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "server-bridge.h"
#include "treeland-window-transition-unstable-v1.h"

#include "core/shellhandler.h"
#include "core/waylandserver.h"
#include "modules/window-transition/windowtransitionmanagerinterfacev1.h"
#include "surface/surfacewindowtransition.h"
#include "surface/surfacewrapper.h"

#include <QList>
#include <QPointer>
#include <QString>
#include <wbackend.h>
#include <wsurface.h>

#include <utility>

namespace {
WindowTransitionManagerInterfaceV1 *g_manager = nullptr;
QList<QPointer<SurfaceWrapper>> g_wrappers;
}

void protocol_test_setup(WaylandServer *server)
{
    g_manager = find_server_interface<WindowTransitionManagerInterfaceV1>(server);
    Q_ASSERT(g_manager);
    add_headless_output(server->backend(), false);
    QObject::connect(server->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     server,
                     [](SurfaceWrapper *wrapper) {
                         g_wrappers.append(QPointer<SurfaceWrapper>(wrapper));
                     });
}

extern "C" void window_transition_read_server_state(void *data)
{
    auto *state = static_cast<struct window_transition_server_state *>(data);
    state->manager_present = g_manager ? 1 : 0;
    state->pending = 0;
    state->armed_count = 0;
    state->rect_present = 0;
    state->rect_mapped = 0;
    state->rect_w = 0;
    state->rect_h = 0;
    state->source_set = 0;
    if (g_manager && state->token[0] != '\0') {
        state->pending = g_manager->hasPendingWindowTransitionRect(
                             QString::fromUtf8(state->token))
                             ? 1
                             : 0;
    }

    // Report the rectangle the production manager associated with a real
    // SurfaceWindowTransition. globalRect() resolves only for a transition that
    // holds the manager's rect, so its value names the armed window instead of
    // trusting the first wrapper that happens to be active.
    for (const auto &wrapper : std::as_const(g_wrappers)) {
        if (!wrapper)
            continue;
        auto *transition =
            qobject_cast<SurfaceWindowTransition *>(wrapper->windowTransition());
        if (!transition)
            continue;
        if (transition->active())
            ++state->armed_count;

        // The rect survives a 0x0 geometry (that only disables it), so presence
        // and size are reported separately from active().
        if (state->rect_present)
            continue;
        const auto rect = transition->globalRect();
        if (!rect)
            continue;
        state->rect_present = 1;
        state->rect_mapped =
            (wrapper->surface() && wrapper->surface()->mapped()) ? 1 : 0;
        state->rect_w = qRound(rect->width());
        state->rect_h = qRound(rect->height());
        state->source_set = transition->sourceSurface() ? 1 : 0;
    }
}
