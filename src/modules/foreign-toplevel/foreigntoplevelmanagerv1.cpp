// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "common/treelandlogging.h"
#include "core/rootsurfacecontainer.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include <woutput.h>
#include <wsocket.h>
#include <wtoplevelsurface.h>
#include <wxdgtoplevelsurface.h>
#include <wxwaylandsurface.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwxdgshell.h>

#include <qassert.h>
#include <qlogging.h>

static ForeignToplevelV1 *FOREIGN_TOPLEVEL_MANAGER = nullptr;

ForeignToplevelV1::ForeignToplevelV1(QObject *parent)
    : QObject(parent)
{
    if (FOREIGN_TOPLEVEL_MANAGER) {
        qCFatal(treelandProtocol) << "There are multiple instances of QuickForeignToplevelManagerV1";
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
    if (m_surfaces.contains(wrapper)) {
        qCCritical(treelandProtocol)
            << wrapper << " has been add to foreign toplevel twice";
        return;
    }

    auto handle = treeland_foreign_toplevel_handle_v1::create(m_manager);
    m_surfaces.insert({ wrapper, std::unique_ptr<treeland_foreign_toplevel_handle_v1>(handle) });

    static int counter = 1;
    handle->set_identifier(
        //     *reinterpret_cast<const uint32_t *>(surface->surface()->handle()->handle()));
        //*reinterpret_cast<const uint32_t *>(wrapper));
        counter++);

    if (wrapper->type() == SurfaceWrapper::Type::SplashScreen) {
        handle->set_title(QString("SplashScreen") + wrapper->appId());
        handle->set_app_id(wrapper->appId());
        handle->set_minimized(false);
        handle->set_maximized(false);
        handle->set_fullscreen(false);
        handle->set_activated(false);

        connect(handle, &treeland_foreign_toplevel_handle_v1::requestClose, wrapper, [wrapper] {
            wrapper->close();
        });

        // Monitor surfaceItemCreated to complete initialization when splash converts to normal
        // window
        connect(wrapper, &SurfaceWrapper::surfaceItemCreated, this, [this, wrapper, handle]() {
            initializeToplevelHandle(wrapper, handle);
        });
        return;
    }

    initializeToplevelHandle(wrapper, handle);
}

void ForeignToplevelV1::initializeToplevelHandle(SurfaceWrapper *wrapper,
                                                 treeland_foreign_toplevel_handle_v1 *handle)
{
    Q_ASSERT(wrapper->type() == SurfaceWrapper::Type::XdgToplevel
             || wrapper->type() == SurfaceWrapper::Type::XWayland);
    auto surface = wrapper->shellSurface();
    qInfo() << "Register surface to ForeignToplevelV1, appId=" << wrapper->appId()
            << wrapper->type() << wrapper->skipDockPreView();

    // initSurface
    handle->set_title(surface->title());
    handle->set_app_id(wrapper->appId());
    handle->set_minimized(surface->isMinimized());
    handle->set_maximized(surface->isMaximized());
    handle->set_fullscreen(surface->isFullScreen());
    handle->set_activated(surface->isActivated());

    surface->safeConnect(&WToplevelSurface::titleChanged, handle, [handle, surface] {
        handle->set_title(surface->title());
    });

    surface->safeConnect(&WToplevelSurface::minimizeChanged, handle, [handle, surface] {
        handle->set_minimized(surface->isMinimized());
    });

    surface->safeConnect(&WToplevelSurface::maximizeChanged, handle, [handle, surface] {
        handle->set_maximized(surface->isMaximized());
    });

    surface->safeConnect(&WToplevelSurface::fullscreenChanged, handle, [handle, surface] {
        handle->set_fullscreen(surface->isFullScreen());
    });

    surface->safeConnect(&WToplevelSurface::activateChanged, handle, [handle, surface] {
        handle->set_activated(surface->isActivated());
    });

    surface->safeConnect(&WToplevelSurface::appIdChanged, handle, [handle, wrapper] {
        handle->set_app_id(wrapper->appId());
    });

    surface->surface()->safeConnect(&WSurface::outputEntered, handle, [handle](WOutput *output) {
        handle->output_enter(output->handle());
    });

    surface->surface()->safeConnect(&WSurface::outputLeave, handle, [handle](WOutput *output) {
        handle->output_leave(output->handle());
    });

    connect(handle,
            &treeland_foreign_toplevel_handle_v1::requestActivate,
            wrapper,
            [wrapper]([[maybe_unused]] treeland_foreign_toplevel_handle_v1_activated_event *event) {
                Helper::instance()->forceActivateSurface(wrapper);
            });

    connect(handle,
            &treeland_foreign_toplevel_handle_v1::requestMaximize,
            wrapper,
            [wrapper](treeland_foreign_toplevel_handle_v1_maximized_event *event) {
                if (event->maximized)
                    wrapper->requestMaximize();
                else
                    wrapper->requestCancelMaximize();
            });

    connect(handle,
            &treeland_foreign_toplevel_handle_v1::requestMinimize,
            wrapper,
            [wrapper](treeland_foreign_toplevel_handle_v1_minimized_event *event) {
                if ((Helper::instance()->showDesktopState()
                     == WindowManagementV1::DesktopState::Show)) {
                    Helper::instance()->forceActivateSurface(wrapper);
                    return;
                }

                if (event->minimized)
                    wrapper->requestMinimize();
                else
                    wrapper->requestCancelMinimize();
            });

    connect(handle,
            &treeland_foreign_toplevel_handle_v1::requestFullscreen,
            wrapper,
            [wrapper](treeland_foreign_toplevel_handle_v1_fullscreen_event *event) {
                if (event->fullscreen)
                    wrapper->requestFullscreen();
                else
                    wrapper->requestCancelFullscreen();
            });

    connect(handle,
            &treeland_foreign_toplevel_handle_v1::rectangleChanged,
            wrapper,
            [wrapper](treeland_foreign_toplevel_handle_v1_set_rectangle_event *event) {
                auto dockWrapper = Helper::instance()->rootSurfaceContainer()->getSurface(
                    WSurface::fromHandle(event->surface));
                wrapper->setIconGeometry(QRect(dockWrapper->x() + event->x,
                                               dockWrapper->y() + event->y,
                                               event->width,
                                               event->height));
            });

    if (auto *xdgSurface = qobject_cast<WXdgToplevelSurface *>(surface)) {
        auto client = WClient::get(xdgSurface->handle()->handle()->resource->client);
        handle->set_pid(client->credentials().get()->pid);

        auto updateSurfaceParent = [this, handle, xdgSurface] {
            WXdgToplevelSurface *p = xdgSurface->parentXdgSurface();
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
            qCCritical(treelandProtocol)
                << "Xdg toplevel surface " << xdgSurface
                << "has set parent surface, but foreign_toplevel_handle for "
                   "parent surface not "
                   "found!";
        };
        xdgSurface->safeConnect(&WXdgToplevelSurface::parentXdgSurfaceChanged,
                                handle,
                                updateSurfaceParent);
        updateSurfaceParent();
    } else if (auto *xwaylandSurface = qobject_cast<WXWaylandSurface *>(surface)) {
        handle->set_pid(xwaylandSurface->pid());

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
            qCCritical(treelandProtocol)
                << "X11 surface " << xwaylandSurface
                << "has set parent surface, but foreign_toplevel_handle for "
                   "parent surface not "
                   "found!";
        };
        xwaylandSurface->safeConnect(&WXWaylandSurface::parentXWaylandSurfaceChanged,
                                     handle,
                                     updateSurfaceParent);
        updateSurfaceParent();
    } else {
        qCFatal(treelandProtocol)
            << "TreelandForeignToplevelManager only support WXdgSurface or "
               "WXWaylandSurface";
    }
}

void ForeignToplevelV1::removeSurface(SurfaceWrapper *wrapper)
{
    qInfo() << "Unregister surface from ForeignToplevelV1, appId=" << wrapper->appId()
            << wrapper->type() << wrapper->skipDockPreView();

    if (!m_surfaces.count(wrapper)) {
        qCCritical(treelandProtocol) << wrapper << " is not registered in foreign toplevel";
        return;
    }
    m_surfaces.erase(wrapper);
}

void ForeignToplevelV1::enterDockPreview(WSurface *relative_surface)
{
    for (auto *context : m_manager->dock_preview) {
        if (context->relative_surface == relative_surface->handle()->handle()) {
            context->enter();
            break;
        }
    }
}

void ForeignToplevelV1::leaveDockPreview(WSurface *relative_surface)
{
    for (auto *context : m_manager->dock_preview) {
        if (context->relative_surface == relative_surface->handle()->handle()) {
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
                for (auto toplevelIt = event->toplevels.cbegin();
                     toplevelIt != event->toplevels.cend();
                     ++toplevelIt) {
                    const uint32_t identifier = *toplevelIt;
                    for (auto surfaceIt = m_surfaces.cbegin(); surfaceIt != m_surfaces.cend();
                         ++surfaceIt) {
                        if (surfaceIt->second->identifier == identifier) {
                            surfaces.push_back(surfaceIt->first);
                            break;
                        }
                    }
                };

                Q_EMIT requestDockPreview(surfaces,
                                          WSurface::fromHandle(event->toplevel->relative_surface),
                                          QPoint(event->x, event->y),
                                          static_cast<PreviewDirection>(event->direction));
            });
    connect(context,
            &treeland_dock_preview_context_v1::requestShowTooltip,
            this,
            [this](treeland_dock_preview_tooltip_event *event) {
                Q_EMIT requestDockPreviewTooltip(
                    event->tooltip,
                    WSurface::fromHandle(event->toplevel->relative_surface),
                    QPoint(event->x, event->y),
                    static_cast<PreviewDirection>(event->direction));
            });
    connect(context,
            &treeland_dock_preview_context_v1::requestClose,
            this,
            &ForeignToplevelV1::requestDockClose);
    connect(context,
            &treeland_dock_preview_context_v1::before_destroy,
            this,
            &ForeignToplevelV1::requestDockClose);
}

QByteArrayView ForeignToplevelV1::interfaceName() const
{
    return "treeland_foreign_toplevel_manager_v1";
}
