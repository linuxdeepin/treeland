// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "foreigntoplevelmanagerv1.h"

#include "server-protocol.h"
#include "impl/foreign_toplevel_manager_impl.h"
#include "impl/foreigntoplevelhandlev1.h"

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-util.h>
#include <woutput.h>
#include <wxdgshell.h>
#include <wxdgsurface.h>
#include <wxwaylandsurface.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwsignalconnector.h>
#include <qwxdgshell.h>

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

QuickForeignToplevelManagerAttached::QuickForeignToplevelManagerAttached(
    WSurface *target, QuickForeignToplevelManagerV1 *manager)
    : QObject(manager)
    , m_target(target)
    , m_manager(manager)
{
    connect(manager,
            &QuickForeignToplevelManagerV1::requestActivate,
            this,
            [this](WToplevelSurface *surface,
                   [[maybe_unused]] treeland_foreign_toplevel_handle_v1_activated_event *event) {
                if (surface->surface() != m_target) {
                    return;
                }

                Q_EMIT requestActivate(true);
            });
    connect(manager,
            &QuickForeignToplevelManagerV1::requestMinimize,
            this,
            [this](WToplevelSurface *surface,
                   treeland_foreign_toplevel_handle_v1_minimized_event *event) {
                if (surface->surface() != m_target) {
                    return;
                }

                Q_EMIT requestMinimize(event->minimized);
            });
    connect(manager,
            &QuickForeignToplevelManagerV1::requestMaximize,
            this,
            [this](WToplevelSurface *surface,
                   treeland_foreign_toplevel_handle_v1_maximized_event *event) {
                if (surface->surface() != m_target) {
                    return;
                }

                Q_EMIT requestMaximize(event->maximized);
            });
    connect(manager,
            &QuickForeignToplevelManagerV1::requestFullscreen,
            this,
            [this](WToplevelSurface *surface,
                   treeland_foreign_toplevel_handle_v1_fullscreen_event *event) {
                if (surface->surface() != m_target) {
                    return;
                }

                Q_EMIT requestFullscreen(event->fullscreen);
            });
    connect(manager,
            &QuickForeignToplevelManagerV1::requestClose,
            this,
            [this](WToplevelSurface *surface) {
                if (surface->surface() != m_target) {
                    return;
                }

                Q_EMIT requestClose();
            });
    connect(manager,
            &QuickForeignToplevelManagerV1::rectangleChanged,
            this,
            [this](WToplevelSurface *surface,
                   treeland_foreign_toplevel_handle_v1_set_rectangle_event *event) {
                if (surface->surface() != m_target) {
                    return;
                }

                Q_EMIT rectangleChanged({ event->x, event->y, event->width, event->height });
            });
}

class QuickForeignToplevelManagerV1Private : public WObjectPrivate
{
public:
    QuickForeignToplevelManagerV1Private(QuickForeignToplevelManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    ~QuickForeignToplevelManagerV1Private() = default;

    void initSurface(WToplevelSurface *surface)
    {
        auto handle = surfaces[surface];
        std::vector<QMetaObject::Connection> connection;

        connection.push_back(
            QObject::connect(surface, &WToplevelSurface::titleChanged, q_func(), [=] {
                handle->setTitle(surface->title().toUtf8());
            }));

        connection.push_back(
            QObject::connect(surface, &WToplevelSurface::appIdChanged, q_func(), [=] {
                handle->setAppId(surface->appId().toUtf8());
            }));

        connection.push_back(
            QObject::connect(surface, &WToplevelSurface::minimizeChanged, q_func(), [=] {
                handle->setMinimized(surface->isMinimized());
            }));

        connection.push_back(
            QObject::connect(surface, &WToplevelSurface::maximizeChanged, q_func(), [=] {
                handle->setMaximized(surface->isMaximized());
            }));

        connection.push_back(
            QObject::connect(surface, &WToplevelSurface::fullscreenChanged, q_func(), [=] {
                handle->setFullScreen(surface->isFullScreen());
            }));

        connection.push_back(
            QObject::connect(surface, &WToplevelSurface::activateChanged, q_func(), [=] {
                handle->setActivated(surface->isActivated());
            }));

        connection.push_back(QObject::connect(
            surface,
            &WToplevelSurface::parentSurfaceChanged,
            q_func(),
            [this, surface, handle] {
                auto find = std::find_if(surfaces.begin(), surfaces.end(), [surface](auto pair) {
                    return pair.first == surface;
                });

                handle->setParent(find != surfaces.end() ? find->second.get() : nullptr);
            }));

        connection.push_back(QObject::connect(surface->surface(),
                                              &WSurface::outputEntered,
                                              q_func(),
                                              [handle](WOutput *output) {
                                                  handle->outputEnter(output->handle());
                                              }));

        connection.push_back(QObject::connect(surface->surface(),
                                              &WSurface::outputLeft,
                                              q_func(),
                                              [handle](WOutput *output) {
                                                  handle->outputLeave(output->handle());
                                              }));

        connection.push_back(QObject::connect(
            handle.get(),
            &TreeLandForeignToplevelHandleV1::requestActivate,
            q_func(),
            [surface, this, handle](treeland_foreign_toplevel_handle_v1_activated_event *event) {
                Q_EMIT q_func()->requestActivate(surface, event);
            }));

        connection.push_back(QObject::connect(
            handle.get(),
            &TreeLandForeignToplevelHandleV1::requestMaximize,
            q_func(),
            [surface, this](treeland_foreign_toplevel_handle_v1_maximized_event *event) {
                Q_EMIT q_func()->requestMaximize(surface, event);
            }));

        connection.push_back(QObject::connect(
            handle.get(),
            &TreeLandForeignToplevelHandleV1::requestMinimize,
            q_func(),
            [surface, this, handle](treeland_foreign_toplevel_handle_v1_minimized_event *event) {
                Q_EMIT q_func()->requestMinimize(surface, event);
            }));

        connection.push_back(QObject::connect(
            handle.get(),
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

        if (auto *xdgSurface = qobject_cast<WXdgSurface *>(surface)) {
            wl_client *client = xdgSurface->handle()->handle()->resource->client;
            pid_t pid;
            wl_client_get_credentials(client, &pid, nullptr, nullptr);
            handle->setPid(pid);
        } else if (auto *xwaylandSurface = qobject_cast<WXWaylandSurface *>(surface)) {
            handle->setPid(xwaylandSurface->pid());
        } else {
            qFatal("TreelandForeignToplevelManager only support WXdgSurface or WXWaylandSurface");
        }

        handle->setIdentifier(
            *reinterpret_cast<const uint32_t *>(surface->surface()->handle()->handle()));

        handle->setTitle(surface->title().toUtf8());
        handle->setAppId(surface->appId().toUtf8());
        handle->setMinimized(surface->isMinimized());
        handle->setMaximized(surface->isMaximized());
        handle->setFullScreen(surface->isFullScreen());
        handle->setActivated(surface->isActivated());

        connections.insert({ surface, connection });
    }

    void add(WToplevelSurface *surface)
    {
        auto handle = std::shared_ptr<TreeLandForeignToplevelHandleV1>(
            TreeLandForeignToplevelHandleV1::create(manager));
        surfaces.insert({ surface, handle });
        initSurface(surface);
    }

    void remove(WToplevelSurface *surface)
    {
        for (auto co : connections[surface]) {
            QObject::disconnect(co);
        }

        connections.erase(surface);
        surfaces.erase(surface);
    }

    void surfaceOutputEnter(WToplevelSurface *surface, WOutput *output)
    {
        auto handle = surfaces[surface];
        handle->outputEnter(output->handle());
    }

    void surfaceOutputLeave(WToplevelSurface *surface, WOutput *output)
    {
        auto handle = surfaces[surface];
        handle->outputLeave(output->handle());
    }

    W_DECLARE_PUBLIC(QuickForeignToplevelManagerV1)

    TreeLandForeignToplevelManagerV1 *manager = nullptr;
    std::map<WToplevelSurface *, std::shared_ptr<TreeLandForeignToplevelHandleV1>> surfaces;
    std::map<WToplevelSurface *, std::vector<QMetaObject::Connection>> connections;
    std::vector<TreeLandDockPreviewContextV1 *> dockPreviews;
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

void QuickForeignToplevelManagerV1::add(WToplevelSurface *surface)
{
    W_D(QuickForeignToplevelManagerV1);
    d->add(surface);
}

void QuickForeignToplevelManagerV1::remove(WToplevelSurface *surface)
{
    W_D(QuickForeignToplevelManagerV1);
    d->remove(surface);
}

void QuickForeignToplevelManagerV1::enterDockPreview(WSurface *relative_surface)
{
    W_D(QuickForeignToplevelManagerV1);

    for (auto *context : d->dockPreviews) {
        if (context->handle()->relative_surface == relative_surface->handle()->handle()->resource) {
            context->enter();
            break;
        }
    }
}

void QuickForeignToplevelManagerV1::leaveDockPreview(WSurface *relative_surface)
{
    W_D(QuickForeignToplevelManagerV1);

    for (auto *context : d->dockPreviews) {
        if (context->handle()->relative_surface == relative_surface->handle()->handle()->resource) {
            context->leave();
            break;
        }
    }
}

void QuickForeignToplevelManagerV1::create()
{
    W_D(QuickForeignToplevelManagerV1);
    WQuickWaylandServerInterface::create();

    d->manager = TreeLandForeignToplevelManagerV1::create(server()->handle());

    connect(d->manager,
            &TreeLandForeignToplevelManagerV1::dockPreviewContextCreated,
            this,
            [this, d](TreeLandDockPreviewContextV1 *context) {
                d->dockPreviews.push_back(context);
                connect(context, &TreeLandDockPreviewContextV1::beforeDestroy, this, [d, context] {
                    std::erase_if(d->dockPreviews, [context](auto *p) {
                        return p == context;
                    });
                });
                connect(
                    context,
                    &TreeLandDockPreviewContextV1::requestShow,
                    this,
                    [this, d](struct treeland_dock_preview_context_v1_preview_event *event) {
                        std::vector<WSurface *> surfaces;
                        for (auto it = event->toplevels.cbegin(); it != event->toplevels.cend();
                             ++it) {
                            const uint32_t identifier = *it;
                            for (auto it = d->surfaces.cbegin(); it != d->surfaces.cend(); ++it) {
                                if (it->second->handle()->identifier == identifier) {
                                    surfaces.push_back(it->first->surface());
                                    break;
                                }
                            }
                        };

                        Q_EMIT requestDockPreview(surfaces,
                                                  WSurface::fromHandle(wlr_surface_from_resource(
                                                      event->toplevel->relative_surface)),
                                                  QPoint(event->x, event->y),
                                                  event->direction);
                    });

                connect(context,
                        &TreeLandDockPreviewContextV1::requestClose,
                        this,
                        &QuickForeignToplevelManagerV1::requestDockClose);
            });
}

QuickForeignToplevelManagerAttached *QuickForeignToplevelManagerV1::qmlAttachedProperties(
    QObject *target)
{
    if (auto *surface = qobject_cast<WXdgSurface *>(target)) {
        return new QuickForeignToplevelManagerAttached(surface->surface(),
                                                       FOREIGN_TOPLEVEL_MANAGER);
    }

    if (auto *surface = qobject_cast<WXWaylandSurface *>(target)) {
        return new QuickForeignToplevelManagerAttached(surface->surface(),
                                                       FOREIGN_TOPLEVEL_MANAGER);
    }

    return nullptr;
}
