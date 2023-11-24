// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "foreigntoplevelmanagerv1.h"

#include "foreign-toplevel-manager-server-protocol.h"
#include "protocols/foreigntoplevelhandlev1.h"

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwsignalconnector.h>
#include <qwxdgshell.h>
#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>

#include <QDebug>
#include <QTimer>

#include <sys/socket.h>
#include <unistd.h>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
}

class QuickForeignToplevelManagerV1Private : public WObjectPrivate {
public:
    QuickForeignToplevelManagerV1Private(QuickForeignToplevelManagerV1 *qq)
        : WObjectPrivate(qq) {}
    ~QuickForeignToplevelManagerV1Private() = default;

    void initSurface(WXdgSurface *surface) {
        auto handle = surfaces[surface];
        std::vector<QMetaObject::Connection> connection;

        connection.push_back(QObject::connect(surface, &WXdgSurface::titleChanged, q_func(),
                         [=] { handle->setTitle(surface->title().toUtf8()); }));

        connection.push_back(QObject::connect(surface, &WXdgSurface::appIdChanged, q_func(),
                         [=] { handle->setAppId(surface->appId().toUtf8()); }));

        connection.push_back(QObject::connect(surface, &WXdgSurface::requestMinimize, q_func(),
                         [=] { handle->setMinimized(surface->isMinimized()); }));

        connection.push_back(QObject::connect(surface, &WXdgSurface::requestMaximize, q_func(),
                         [=] { handle->setMaximized(surface->isMaximized()); }));

        connection.push_back(QObject::connect(
            surface, &WXdgSurface::requestFullscreen, q_func(),
            [=] { handle->setFullScreen(surface->isFullScreen()); }));

        connection.push_back(QObject::connect(surface, &WXdgSurface::activateChanged, q_func(),
                         [=] { handle->setActivated(surface->isActivated()); }));

        connection.push_back(QObject::connect(
            surface, &WXdgSurface::parentSurfaceChanged, q_func(),
            [this, surface, handle] {
                auto find = std::find_if(
                    surfaces.begin(), surfaces.end(),
                    [surface](auto pair) { return pair.first == surface; });

                handle->setParent(find != surfaces.end() ? find->second.get()
                                                         : nullptr);
            }));

        connection.push_back(QObject::connect(surface->surface(), &WSurface::outputEntered, q_func(), [handle](WOutput *output) {
            handle->outputEnter(output->handle());
        }));

        connection.push_back(QObject::connect(surface->surface(), &WSurface::outputLeft, q_func(), [handle](WOutput *output) {
            handle->outputLeave(output->handle());
        }));

        connection.push_back(QObject::connect(handle.get(),
                             &TreeLandForeignToplevelHandleV1::requestActivate,
                             surface,
                             [surface](treeland_foreign_toplevel_handle_v1_activated_event *event) {
                                 surface->setActivate(event->toplevel->state & TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED);
                             }));

        connection.push_back(QObject::connect(handle.get(),
                             &TreeLandForeignToplevelHandleV1::requestMaximize,
                             surface,
                             [surface](treeland_foreign_toplevel_handle_v1_maximized_event *event) {
                                 surface->setMaximize(event->maximized);
                             }));

        connection.push_back(QObject::connect(handle.get(),
                             &TreeLandForeignToplevelHandleV1::requestMinimize,
                             surface,
                             [surface](treeland_foreign_toplevel_handle_v1_minimized_event *event) {
                                 surface->setMinimize(event->minimized);
                             }));

        connection.push_back(QObject::connect(handle.get(),
                             &TreeLandForeignToplevelHandleV1::requestFullscreen,
                             surface,
                             [surface](treeland_foreign_toplevel_handle_v1_fullscreen_event *event) {
                                 surface->setFullScreen(event->fullscreen);
                             }));

        connection.push_back(QObject::connect(handle.get(),
                             &TreeLandForeignToplevelHandleV1::requestClose,
                             surface,
                             [surface] {
                                 surface->handle()->topToplevel()->sendClose();
                             }));

        wl_client *client = surface->handle()->handle()->resource->client;
        pid_t pid;
        wl_client_get_credentials(client, &pid, nullptr, nullptr);
        handle->setPid(pid);

        handle->setIdentifier(QString::number(reinterpret_cast<std::uintptr_t>(surface)).toUtf8());

        Q_EMIT surface->titleChanged();
        Q_EMIT surface->appIdChanged();
        Q_EMIT surface->requestMinimize();
        Q_EMIT surface->requestMaximize();
        Q_EMIT surface->requestFullscreen();
        Q_EMIT surface->activateChanged();

        connections.insert({surface, connection});
    }

    void add(WXdgSurface *surface) {
        auto handle = std::shared_ptr<TreeLandForeignToplevelHandleV1>(TreeLandForeignToplevelHandleV1::create(manager));
        surfaces.insert({surface, handle});
        initSurface(surface);
    }

    void remove(WXdgSurface *surface) {
        for (auto co : connections[surface]) {
            QObject::disconnect(co);
        }

        connections.erase(surface);
        surfaces.erase(surface);
    }

    void surfaceOutputEnter(WXdgSurface *surface, WOutput *output) {
        auto handle = surfaces[surface];
        handle->outputEnter(output->handle());
    }

    void surfaceOutputLeave(WXdgSurface *surface, WOutput *output) {
        auto handle = surfaces[surface];
        handle->outputLeave(output->handle());
    }

    W_DECLARE_PUBLIC(QuickForeignToplevelManagerV1)

    TreeLandForeignToplevelManagerV1 *manager = nullptr;
    std::map<WXdgSurface*, std::shared_ptr<TreeLandForeignToplevelHandleV1>> surfaces;
    std::map<WXdgSurface*, std::vector<QMetaObject::Connection>> connections;
};

QuickForeignToplevelManagerV1::QuickForeignToplevelManagerV1(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new QuickForeignToplevelManagerV1Private(this), nullptr) {}

void QuickForeignToplevelManagerV1::add(WXdgSurface *surface) {
    W_D(QuickForeignToplevelManagerV1);
    d->add(surface);
}

void QuickForeignToplevelManagerV1::remove(WXdgSurface *surface) {
    W_D(QuickForeignToplevelManagerV1);
    d->remove(surface);
}

void QuickForeignToplevelManagerV1::create() {
    W_D(QuickForeignToplevelManagerV1);
    WQuickWaylandServerInterface::create();

    d->manager = TreeLandForeignToplevelManagerV1::create(server()->handle());
}
