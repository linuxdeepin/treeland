// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wayland-ext-session-lock-v1.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "output/output.h"
#include "surface/surfacecontainer.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>
#include <protocols/wsessionlockmanager.h>
#include <protocols/wsessionlock.h>

#include <QtMath>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
// The WSessionLockManager resolved from the server interface list.
WSessionLockManager *g_manager = nullptr;
// The WSessionLock captured from the manager's lockCreated signal.
WSessionLock *g_lock = nullptr;
// The client's exclusive-zone panel (top layer surface).
SurfaceWrapper *g_panelWrapper = nullptr;

// Session lock surfaces are created by the LockScreen container itself, not by
// ShellHandler, so the wrapper is looked up through that container.
SurfaceWrapper *lockWrapper()
{
    auto *root = WaylandServer::instance()->rootSurfaceContainer();
    auto *lockScreen = root->findChild<SurfaceContainer *>(QStringLiteral("LockScreenContainer"));
    if (!lockScreen)
        return nullptr;
    for (auto *surface : lockScreen->surfaces()) {
        if (surface->type() == SurfaceWrapper::Type::LockScreen)
            return surface;
    }
    return nullptr;
}

// The output the client put its surfaces on: the lock surface's output once it
// exists, otherwise the panel's. Both come from the same wl_output.
Output *testOutput()
{
    if (auto *wrapper = lockWrapper(); wrapper && wrapper->ownsOutput())
        return wrapper->ownsOutput();
    if (g_panelWrapper && g_panelWrapper->ownsOutput())
        return g_panelWrapper->ownsOutput();
    return nullptr;
}
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);

    g_manager = find_server_interface<WSessionLockManager>(server);
    if (g_manager) {
        QObject::connect(g_manager, &WSessionLockManager::lockCreated,
                         server, [](WSessionLock *lock) {
                             g_lock = lock;
                         });
    }

    // Same capture pattern as the wlr-layer-shell test: keep the wrapper the
    // production path created for the client's panel layer surface.
    QObject::connect(server->shellHandler(),
                     &ShellHandler::surfaceWrapperAdded,
                     server,
                     [](SurfaceWrapper *wrapper) {
                         if (wrapper->type() == SurfaceWrapper::Type::Layer)
                             g_panelWrapper = wrapper;
                     });
}

// E-level read: the production WSessionLock state plus the geometry the lock
// surface wrapper ended up with, so the client can assert a lock surface covers
// the whole output instead of the exclusive-zone-free work area.
void session_lock_read_server_state(void *data)
{
    auto *state = static_cast<session_lock_server_state *>(data);
    *state = session_lock_server_state {};

    state->valid = g_lock ? 1 : 0;
    state->locked = (g_lock && g_lock->isLocked()) ? 1 : 0;

    auto *output = testOutput();
    if (!output)
        return;

    const QRectF outputGeometry = output->geometry();
    state->output_width = qRound(outputGeometry.width());
    state->output_height = qRound(outputGeometry.height());
    state->output_x = qRound(outputGeometry.x());
    state->output_y = qRound(outputGeometry.y());
    state->valid_area_height = qRound(output->validGeometry().height());

    auto *wrapper = lockWrapper();
    if (!wrapper)
        return;
    state->wrapper_valid = 1;
    state->wrapper_x = qRound(wrapper->x());
    state->wrapper_y = qRound(wrapper->y());
    state->wrapper_width = qRound(wrapper->width());
    state->wrapper_height = qRound(wrapper->height());
}
