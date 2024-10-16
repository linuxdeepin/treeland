// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"
#include "rootsurfacecontainer.h"
#include "surfacewrapper.h"

#include <woutput.h>
#include <wsocket.h>
#include <wtoplevelsurface.h>
#include <wxdgsurface.h>
#include <wxwaylandsurface.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwxdgshell.h>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(qLcTreelandForeignToplevel, "treeland.protocols.foreigntoplevel", QtWarningMsg)

static ForeignToplevelV1 *FOREIGN_TOPLEVEL_MANAGER = nullptr;

ForeignToplevelV1::ForeignToplevelV1(QObject *parent)
    : QObject(parent)
{
    if (FOREIGN_TOPLEVEL_MANAGER) {
        qFatal("There are multiple instances of QuickForeignToplevelManagerV1");
    }

    FOREIGN_TOPLEVEL_MANAGER = this;
}

void ForeignToplevelV1::create(WServer *server)
{
    m_manager = treeland_foreign_toplevel_manager_v1::create(server->handle());

    connect(m_manager,
            &treeland_foreign_toplevel_manager_v1::dockPreviewContextCreated,
            this,
            &ForeignToplevelV1::onDockPreviewContextCreated);
}

wl_global *ForeignToplevelV1::global() const
{
    return m_manager->global;
}

void ForeignToplevelV1::addSurface(SurfaceWrapper *wrapper)
{
    Q_ASSERT(wrapper->type() == SurfaceWrapper::Type::XdgToplevel
             || wrapper->type() == SurfaceWrapper::Type::XWayland);

    auto handle = treeland_foreign_toplevel_handle_v1::create(m_manager);
    m_surfaces.insert({ wrapper, std::unique_ptr<treeland_foreign_toplevel_handle_v1>(handle) });
    auto surface = wrapper->shellSurface();

    // initSurface
    std::vector<QMetaObject::Connection> connection;

    connection.push_back(
        surface->safeConnect(&WToplevelSurface::titleChanged, this, [handle, surface] {
            handle->set_title(surface->title());
        }));

    connection.push_back(
        surface->safeConnect(&WToplevelSurface::minimizeChanged, this, [handle, surface] {
            handle->set_minimized(surface->isMinimized());
        }));

    connection.push_back(
        surface->safeConnect(&WToplevelSurface::maximizeChanged, this, [handle, surface] {
            handle->set_maximized(surface->isMaximized());
        }));

    connection.push_back(
        surface->safeConnect(&WToplevelSurface::fullscreenChanged, this, [handle, surface] {
            handle->set_fullscreen(surface->isFullScreen());
        }));

    connection.push_back(
        surface->safeConnect(&WToplevelSurface::activateChanged, this, [handle, surface] {
            handle->set_activated(surface->isActivated());
        }));

    connection.push_back(
        connect(surface->surface(), &WSurface::outputEntered, this, [handle](WOutput *output) {
            handle->output_enter(output->handle());
        }));

    connection.push_back(
        connect(surface->surface(), &WSurface::outputLeft, this, [handle](WOutput *output) {
            handle->output_leave(output->handle());
        }));

    connection.push_back(
        connect(handle,
                &treeland_foreign_toplevel_handle_v1::requestActivate,
                this,
                [wrapper, this](treeland_foreign_toplevel_handle_v1_activated_event *event) {
                    Q_EMIT wrapper->requestForceActive();
                }));

    connection.push_back(
        connect(handle,
                &treeland_foreign_toplevel_handle_v1::requestMaximize,
                this,
                [wrapper, this](treeland_foreign_toplevel_handle_v1_maximized_event *event) {
                    if (event->maximized)
                        wrapper->requestMaximize();
                    else
                        wrapper->requestCancelMaximize();
                }));

    connection.push_back(
        connect(handle,
                &treeland_foreign_toplevel_handle_v1::requestMinimize,
                this,
                [wrapper, this](treeland_foreign_toplevel_handle_v1_minimized_event *event) {
                    if (event->minimized)
                        wrapper->requestMinimize();
                    else
                        wrapper->requestCancelMinimize();
                }));

    connection.push_back(
        connect(handle,
                &treeland_foreign_toplevel_handle_v1::requestFullscreen,
                this,
                [wrapper, this](treeland_foreign_toplevel_handle_v1_fullscreen_event *event) {
                    if (event->fullscreen)
                        wrapper->requestFullscreen();
                    else
                        wrapper->requestCancelFullscreen();
                }));

    connection.push_back(
        connect(handle, &treeland_foreign_toplevel_handle_v1::requestClose, this, [surface, this] {
            surface->close();
        }));
    connection.push_back(
        connect(handle,
                &treeland_foreign_toplevel_handle_v1::rectangleChanged,
                this,
                [wrapper, this](treeland_foreign_toplevel_handle_v1_set_rectangle_event *event) {
                    auto dockWrapper = Helper::instance()->rootContainer()->getSurface(
                        WSurface::fromHandle(event->surface));
                    wrapper->setIconGeometry(QRect(dockWrapper->x() + event->x,
                                                   dockWrapper->y() + event->y,
                                                   event->width,
                                                   event->height));
                }));

    if (auto *xdgSurface = qobject_cast<WXdgSurface *>(surface)) {
        auto client = WClient::get(xdgSurface->handle()->handle()->resource->client);
        handle->set_pid(client->credentials().get()->pid);
        connection.push_back(
            surface->safeConnect(&WToplevelSurface::appIdChanged, this, [handle, xdgSurface] {
                auto client = WClient::get(xdgSurface->handle()->handle()->resource->client);
                handle->set_pid(client->credentials().get()->pid);
            }));

        auto updateSurfaceParent = [this, handle, xdgSurface] {
            WToplevelSurface *p = xdgSurface->parentXdgSurface();
            if (!p) {
                handle->set_parent(nullptr);
                return;
            }
            for (const auto &[wrapper, phandle] : m_surfaces) {
                if (wrapper->shellSurface() == p) {
                    handle->set_parent(phandle.get());
                    return;
                }
            }
            qCCritical(qLcTreelandForeignToplevel)
                << "Xdg toplevel surface " << xdgSurface
                << "has set parent surface, but foreign_toplevel_handle for parent surface not "
                   "found!";
        };
        connection.push_back(xdgSurface->safeConnect(&WXdgSurface::parentXdgSurfaceChanged,
                                                     surface,
                                                     updateSurfaceParent));
        updateSurfaceParent();
    } else if (auto *xwaylandSurface = qobject_cast<WXWaylandSurface *>(surface)) {
        handle->set_pid(xwaylandSurface->pid());
        connection.push_back(
            surface->safeConnect(&WToplevelSurface::appIdChanged, this, [handle, surface] {
                handle->set_app_id(surface->appId());
            }));

        auto updateSurfaceParent = [this, handle, xwaylandSurface] {
            WToplevelSurface *p = xwaylandSurface->parentXWaylandSurface();
            if (!p) {
                handle->set_parent(nullptr);
                return;
            }
            for (const auto &[wrapper, phandle] : m_surfaces) {
                if (wrapper->shellSurface() == p) {
                    handle->set_parent(phandle.get());
                    return;
                }
            }
            qCCritical(qLcTreelandForeignToplevel)
                << "X11 surface " << xwaylandSurface
                << "has set parent surface, but foreign_toplevel_handle for parent surface not "
                   "found!";
        };
        connection.push_back(xwaylandSurface->safeConnect(&WXWaylandSurface::parentXWaylandSurface,
                                                          surface,
                                                          updateSurfaceParent));
        updateSurfaceParent();
    } else {
        qCFatal(qLcTreelandForeignToplevel)
            << "TreelandForeignToplevelManager only support WXdgSurface or WXWaylandSurface";
    }

    handle->set_identifier(
        *reinterpret_cast<const uint32_t *>(surface->surface()->handle()->handle()));

    handle->set_title(surface->title());
    handle->set_app_id(surface->appId());
    handle->set_minimized(surface->isMinimized());
    handle->set_maximized(surface->isMaximized());
    handle->set_fullscreen(surface->isFullScreen());
    handle->set_activated(surface->isActivated());

    m_connections.insert({ wrapper, connection });
}

void ForeignToplevelV1::removeSurface(SurfaceWrapper *wrapper)
{
    if (!m_surfaces.count(wrapper)) {
        return;
    }

    for (auto co : m_connections[wrapper]) {
        QObject::disconnect(co);
    }

    m_connections.erase(wrapper);
    m_surfaces.erase(wrapper);
}

void ForeignToplevelV1::enterDockPreview(WSurface *relative_surface)
{
    for (auto *context : m_manager->dock_preview) {
        if (context->relative_surface == relative_surface->handle()->handle()->resource) {
            context->enter();
            break;
        }
    }
}

void ForeignToplevelV1::leaveDockPreview(WSurface *relative_surface)
{
    for (auto *context : m_manager->dock_preview) {
        if (context->relative_surface == relative_surface->handle()->handle()->resource) {
            context->leave();
            break;
        }
    }
}

void ForeignToplevelV1::onDockPreviewContextCreated(treeland_dock_preview_context_v1 *context)
{
    connect(context,
            &treeland_dock_preview_context_v1::requestShow,
            this,
            [this](treeland_dock_preview_context_v1_preview_event *event) {
                std::vector<SurfaceWrapper *> surfaces;
                for (auto it = event->toplevels.cbegin(); it != event->toplevels.cend(); ++it) {
                    const uint32_t identifier = *it;
                    for (auto it = m_surfaces.cbegin(); it != m_surfaces.cend(); ++it) {
                        if (it->second->identifier == identifier) {
                            surfaces.push_back(it->first);
                            break;
                        }
                    }
                };

                Q_EMIT requestDockPreview(surfaces,
                                          WSurface::fromHandle(wlr_surface_from_resource(
                                              event->toplevel->relative_surface)),
                                          QPoint(event->x, event->y),
                                          static_cast<PreviewDirection>(event->direction));
            });
    connect(context,
            &treeland_dock_preview_context_v1::requestShowTooltip,
            this,
            [this](treeland_dock_preview_tooltip_event *event) {
                Q_EMIT requestDockPreviewTooltip(event->tooltip,
                                                 WSurface::fromHandle(wlr_surface_from_resource(
                                                     event->toplevel->relative_surface)),
                                                 QPoint(event->x, event->y),
                                                 static_cast<PreviewDirection>(event->direction));
            });
    connect(context,
            &treeland_dock_preview_context_v1::requestClose,
            this,
            &ForeignToplevelV1::requestDockClose);
}

QByteArrayView ForeignToplevelV1::interfaceName() const
{
    return "treeland_foreign_toplevel_manager_v1";
}
