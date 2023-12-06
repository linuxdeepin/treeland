// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "foreigntoplevelmanagerv1.h"

#include "foreign-toplevel-manager-server-protocol.h"
#include "foreigntoplevelhandlev1.h"

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwsignalconnector.h>
#include <qwxdgshell.h>
#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>
#include <woutput.h>

#include <QDebug>
#include <QTimer>

#include <sys/socket.h>
#include <unistd.h>

extern "C" {
#define static
#include <wlr/types/wlr_xdg_shell.h>
#undef static
}

static QuickForeignToplevelManagerV1 *FOREIGN_TOPLEVEL_MANAGER = nullptr;

QuickForeignToplevelManagerAttached::QuickForeignToplevelManagerAttached(WSurface *target, QuickForeignToplevelManagerV1 *manager)
    : QObject(manager)
    , m_target(target)
    , m_manager(manager)
{
    connect(manager, &QuickForeignToplevelManagerV1::requestActivate, this, [this](WXdgSurface *surface, [[maybe_unused]] treeland_foreign_toplevel_handle_v1_activated_event *event) {
        if (surface->surface() != m_target) {
            return;
        }

        Q_EMIT requestActivate(true);
    });
    connect(manager, &QuickForeignToplevelManagerV1::requestMinimize, this, [this](WXdgSurface *surface, treeland_foreign_toplevel_handle_v1_minimized_event *event) {
        if (surface->surface() != m_target) {
            return;
        }

        Q_EMIT requestMinimize(event->minimized);
    });
    connect(manager, &QuickForeignToplevelManagerV1::requestMaximize, this, [this](WXdgSurface *surface, treeland_foreign_toplevel_handle_v1_maximized_event *event) {
        if (surface->surface() != m_target) {
            return;
        }

        Q_EMIT requestMaximize(event->maximized);
    });
    connect(manager, &QuickForeignToplevelManagerV1::requestFullscreen, this, [this](WXdgSurface *surface, treeland_foreign_toplevel_handle_v1_fullscreen_event *event) {
        if (surface->surface() != m_target) {
            return;
        }

        Q_EMIT requestFullscreen(event->fullscreen);
    });
    connect(manager, &QuickForeignToplevelManagerV1::requestClose, this, [this](WXdgSurface *surface) {
        if (surface->surface() != m_target) {
            return;
        }

        Q_EMIT requestClose();
    });
    connect(manager, &QuickForeignToplevelManagerV1::rectangleChanged, this, [this](WXdgSurface *surface, treeland_foreign_toplevel_handle_v1_set_rectangle_event *event) {
        if (surface->surface() != m_target) {
            return;
        }

        Q_EMIT rectangleChanged({event->x, event->y, event->width, event->height});
    });
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

        connection.push_back(QObject::connect(surface, &WXdgSurface::minimizeChanged, q_func(),
                         [=] { handle->setMinimized(surface->isMinimized()); }));

        connection.push_back(QObject::connect(surface, &WXdgSurface::maximizeChanged, q_func(),
                         [=] { handle->setMaximized(surface->isMaximized()); }));

        connection.push_back(QObject::connect(
            surface, &WXdgSurface::fullscreenChanged, q_func(),
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
                             q_func(),
                             [surface, this, handle](treeland_foreign_toplevel_handle_v1_activated_event *event) {
                                 Q_EMIT q_func()->requestActivate(surface, event);
                             }));

        connection.push_back(QObject::connect(handle.get(),
                             &TreeLandForeignToplevelHandleV1::requestMaximize,
                             q_func(),
                             [surface, this](treeland_foreign_toplevel_handle_v1_maximized_event *event) {
                                 Q_EMIT q_func()->requestMaximize(surface, event);
                             }));

        connection.push_back(QObject::connect(handle.get(),
                             &TreeLandForeignToplevelHandleV1::requestMinimize,
                             q_func(),
                             [surface, this, handle](treeland_foreign_toplevel_handle_v1_minimized_event *event) {
                                 Q_EMIT q_func()->requestMinimize(surface, event);
                             }));

        connection.push_back(QObject::connect(handle.get(),
                             &TreeLandForeignToplevelHandleV1::requestFullscreen,
                             q_func(),
                             [surface, this](treeland_foreign_toplevel_handle_v1_fullscreen_event *event) {
                                 Q_EMIT q_func()->requestFullscreen(surface, event);
                             }));

        connection.push_back(QObject::connect(handle.get(),
                             &TreeLandForeignToplevelHandleV1::requestClose,
                             q_func(),
                             [surface, this] {
                                 Q_EMIT q_func()->requestClose(surface);
                             }));

        wl_client *client = surface->handle()->handle()->resource->client;
        pid_t pid;
        wl_client_get_credentials(client, &pid, nullptr, nullptr);
        handle->setPid(pid);

        handle->setIdentifier(QString::number(reinterpret_cast<std::uintptr_t>(surface)).toUtf8());

        Q_EMIT surface->titleChanged();
        Q_EMIT surface->appIdChanged();
        Q_EMIT surface->minimizeChanged();
        Q_EMIT surface->maximizeChanged();
        Q_EMIT surface->fullscreenChanged();
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
    , WObject(*new QuickForeignToplevelManagerV1Private(this), nullptr)
{
    if (FOREIGN_TOPLEVEL_MANAGER) {
        qFatal("There are multiple instances of QuickForeignToplevelManagerV1");
    }

    FOREIGN_TOPLEVEL_MANAGER = this;
}

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

QuickForeignToplevelManagerAttached *QuickForeignToplevelManagerV1::qmlAttachedProperties(QObject *target)
{
    if (auto *surface = qobject_cast<WXdgSurface*>(target)) {
        return new QuickForeignToplevelManagerAttached(surface->surface(), FOREIGN_TOPLEVEL_MANAGER);
    }

    return nullptr;
}
