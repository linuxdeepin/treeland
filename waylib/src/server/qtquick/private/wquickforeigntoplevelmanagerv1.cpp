// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wglobal.h"
#include "woutput.h"
#include "wquickforeigntoplevelmanagerv1_p.h"
#include "wxdgsurface.h"
#include "private/wglobal_p.h"

#include <qwforeigntoplevelhandlev1.h>
#include <qwxdgshell.h>

#include <map>
#include <memory>

extern "C" {
#define static
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#undef static
}

QW_USE_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

static WQuickForeignToplevelManagerV1 *FOREIGN_TOPLEVEL_MANAGER = nullptr;

WQuickForeignToplevelManagerAttached::WQuickForeignToplevelManagerAttached(WSurface *target, WQuickForeignToplevelManagerV1 *manager)
    : QObject(manager)
    , m_target(target)
    , m_manager(manager)
{
    connect(manager, &WQuickForeignToplevelManagerV1::requestActivate, this, [this](WXdgSurface *surface, [[maybe_unused]] wlr_foreign_toplevel_handle_v1_activated_event *event) {
        if (surface->surface() != m_target) {
            return;
        }

        Q_EMIT requestActivate(true);
    });
    connect(manager, &WQuickForeignToplevelManagerV1::requestMinimize, this, [this](WXdgSurface *surface, wlr_foreign_toplevel_handle_v1_minimized_event *event) {
        if (surface->surface() != m_target) {
            return;
        }

        Q_EMIT requestMinimize(event->minimized);
    });
    connect(manager, &WQuickForeignToplevelManagerV1::requestMaximize, this, [this](WXdgSurface *surface, wlr_foreign_toplevel_handle_v1_maximized_event *event) {
        if (surface->surface() != m_target) {
            return;
        }

        Q_EMIT requestMaximize(event->maximized);
    });
    connect(manager, &WQuickForeignToplevelManagerV1::requestFullscreen, this, [this](WXdgSurface *surface, wlr_foreign_toplevel_handle_v1_fullscreen_event *event) {
        if (surface->surface() != m_target) {
            return;
        }

        Q_EMIT requestFullscreen(event->fullscreen);
    });
    connect(manager, &WQuickForeignToplevelManagerV1::requestClose, this, [this](WXdgSurface *surface) {
        if (surface->surface() != m_target) {
            return;
        }

        Q_EMIT requestClose();
    });
    connect(manager, &WQuickForeignToplevelManagerV1::rectangleChanged, this, [this](WXdgSurface *surface, wlr_foreign_toplevel_handle_v1_set_rectangle_event *event) {
        if (surface->surface() != m_target) {
            return;
        }

        Q_EMIT rectangleChanged({event->x, event->y, event->width, event->height});
    });
}


class WQuickForeignToplevelManagerV1Private : public WObjectPrivate {
public:
    WQuickForeignToplevelManagerV1Private(WQuickForeignToplevelManagerV1 *qq)
        : WObjectPrivate(qq) {}
    ~WQuickForeignToplevelManagerV1Private() = default;

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

        connection.push_back(QObject::connect(surface->surface(), &WSurface::outputEntered, q_func(), [this, handle](WOutput *output) {
            handle->outputEnter(output->handle());
        }));

        connection.push_back(QObject::connect(surface->surface(), &WSurface::outputLeft, q_func(), [this, handle](WOutput *output) {
            handle->outputLeave(output->handle());
        }));

        connection.push_back(QObject::connect(handle.get(),
                             &QWForeignToplevelHandleV1::requestActivate,
                             q_func(),
                             [surface, this](wlr_foreign_toplevel_handle_v1_activated_event *event) {
                                 Q_EMIT q_func()->requestActivate(surface, event);
                             }));

        connection.push_back(QObject::connect(handle.get(),
                             &QWForeignToplevelHandleV1::requestMaximize,
                             q_func(),
                             [surface, this](wlr_foreign_toplevel_handle_v1_maximized_event *event) {
                                 Q_EMIT q_func()->requestMaximize(surface, event);
                             }));

        connection.push_back(QObject::connect(handle.get(),
                             &QWForeignToplevelHandleV1::requestMinimize,
                             q_func(),
                             [surface, this](wlr_foreign_toplevel_handle_v1_minimized_event *event) {
                                 Q_EMIT q_func()->requestMinimize(surface, event);
                             }));

        connection.push_back(QObject::connect(handle.get(),
                             &QWForeignToplevelHandleV1::requestFullscreen,
                             q_func(),
                             [surface, this](wlr_foreign_toplevel_handle_v1_fullscreen_event *event) {
                                 Q_EMIT q_func()->requestFullscreen(surface, event);
                             }));

        connection.push_back(QObject::connect(handle.get(),
                             &QWForeignToplevelHandleV1::requestClose,
                             q_func(),
                             [surface, this] {
                                 Q_EMIT q_func()->requestClose(surface);
                             }));

        Q_EMIT surface->titleChanged();
        Q_EMIT surface->appIdChanged();
        Q_EMIT surface->minimizeChanged();
        Q_EMIT surface->maximizeChanged();
        Q_EMIT surface->fullscreenChanged();
        Q_EMIT surface->activateChanged();
        Q_EMIT surface->parentSurfaceChanged();

        connections.insert({surface, connection});
    }

    void add(WXdgSurface *surface) {
        auto handle = std::shared_ptr<QWForeignToplevelHandleV1>(QWForeignToplevelHandleV1::create(manager));
        surfaces.insert({surface, handle});
        initSurface(surface);
    }

    void remove(WXdgSurface *surface) {
        Q_ASSERT(connections.count(surface));

        for (auto co : connections[surface]) {
            QObject::disconnect(co);
        }

        connections.erase(surface);
        surfaces.erase(surface);
    }

    void surfaceOutputEnter(WXdgSurface *surface, WOutput *output) {
        Q_ASSERT(surfaces.count(surface));
        auto handle = surfaces[surface];
        handle->outputEnter(output->handle());
    }

    void surfaceOutputLeave(WXdgSurface *surface, WOutput *output) {
        Q_ASSERT(surfaces.count(surface));
        auto handle = surfaces[surface];
        handle->outputLeave(output->handle());
    }

    W_DECLARE_PUBLIC(WQuickForeignToplevelManagerV1)

    QWForeignToplevelManagerV1 *manager = nullptr;
    std::map<WXdgSurface*, std::shared_ptr<QWForeignToplevelHandleV1>> surfaces;
    std::map<WXdgSurface*, std::vector<QMetaObject::Connection>> connections;
};

WQuickForeignToplevelManagerV1::WQuickForeignToplevelManagerV1(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new WQuickForeignToplevelManagerV1Private(this), nullptr)
{
    if (FOREIGN_TOPLEVEL_MANAGER) {
        qFatal("There are multiple instances of WQuickForeignToplevelManagerV1");
    }

    FOREIGN_TOPLEVEL_MANAGER = this;
}

void WQuickForeignToplevelManagerV1::add(WXdgSurface *surface) {
    W_D(WQuickForeignToplevelManagerV1);
    d->add(surface);
}

void WQuickForeignToplevelManagerV1::remove(WXdgSurface *surface) {
    W_D(WQuickForeignToplevelManagerV1);
    d->remove(surface);
}

WServerInterface *WQuickForeignToplevelManagerV1::create() {
    W_D(WQuickForeignToplevelManagerV1);

    d->manager = QWForeignToplevelManagerV1::create(server()->handle());

    return new WServerInterface(d->manager, d->manager->handle()->global);
}

WQuickForeignToplevelManagerAttached *WQuickForeignToplevelManagerV1::qmlAttachedProperties(QObject *target)
{
    if (auto *surface = qobject_cast<WXdgSurface*>(target)) {
        return new WQuickForeignToplevelManagerAttached(surface->surface(), FOREIGN_TOPLEVEL_MANAGER);
    }

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
