// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "dockpreviewcontextv3.h"
#include "foreigntoplevelhandlev3.h"
#include "foreigntoplevelmanagerv3.h"
#include "common/treelandlogging.h"
#include "core/rootsurfacecontainer.h"
#include "core/waylandserver.h"
#include "surface/surfacewrapper.h"
#include <wscoplistener.h>

#include "qwayland-server-treeland-foreign-toplevel-manager-unstable-v3.h"

#include <WOutput>
#include <WSeat>
#include <WSurface>

#include <algorithm>
#include <map>
#include <utility>

#include <wayland-server.h>
#include <woutput.h>
#include <wsocket.h>
#include <wtoplevelsurface.h>
#include <wxdgtoplevelsurface.h>
#include <wxwaylandsurface.h>

#include <QPointer>
#include <QVariant>

namespace {
constexpr char DockPreviewContextPropertyName[] = "treelandDockPreviewContextV3";

QByteArray encodeStates(ForeignToplevelHandleV3::States states)
{
    QByteArray ba;
    auto push = [&](uint32_t v) {
        ba.append(reinterpret_cast<const char *>(&v), sizeof(v));
    };

    if (states.testFlag(ForeignToplevelHandleV3::State::Maximized))
        push(TREELAND_FOREIGN_TOPLEVEL_HANDLE_V3_STATE_MAXIMIZED);

    if (states.testFlag(ForeignToplevelHandleV3::State::Minimized))
        push(TREELAND_FOREIGN_TOPLEVEL_HANDLE_V3_STATE_MINIMIZED);

    if (states.testFlag(ForeignToplevelHandleV3::State::Activated))
        push(TREELAND_FOREIGN_TOPLEVEL_HANDLE_V3_STATE_ACTIVATED);

    if (states.testFlag(ForeignToplevelHandleV3::State::Fullscreen))
        push(TREELAND_FOREIGN_TOPLEVEL_HANDLE_V3_STATE_FULLSCREEN);

    if (states.testFlag(ForeignToplevelHandleV3::State::Attention))
        push(TREELAND_FOREIGN_TOPLEVEL_HANDLE_V3_STATE_ATTENTION);

    return ba;
}
}

class SurfaceEntry
{
public:
    SurfaceWrapper *wrapper = nullptr;
    QList<ForeignToplevelHandleV3 *> handles;
};

struct foreign_toplevel_output {
    WOutput *output = nullptr;
    ForeignToplevelHandleV3 *toplevel = nullptr;
};

static DockPreviewContextV3 *dockPreviewContextForSurface(WSurface *relativeSurface)
{
    if (!relativeSurface) {
        return nullptr;
    }

    auto *contextObject = relativeSurface->property(DockPreviewContextPropertyName).value<QObject *>();
    return qobject_cast<DockPreviewContextV3 *>(contextObject);
}

class ForeignToplevelManagerInterfaceV3Private
    : public QtWaylandServer::treeland_foreign_toplevel_manager_v3
{
public:
    ForeignToplevelManagerInterfaceV3Private(ForeignToplevelManagerInterfaceV3 *_q);
    ~ForeignToplevelManagerInterfaceV3Private() override = default;

    ForeignToplevelManagerInterfaceV3 *q = nullptr;
    wl_event_loop *event_loop = nullptr;

    wl_global *global() const;
    std::map<SurfaceWrapper *, std::unique_ptr<SurfaceEntry>> m_surfaces;
    QList<DockPreviewContextV3 *> dockPreviewContexts;
    QList<ForeignToplevelHandleV3 *> handles;
    uint32_t m_nextIdentifier = 1;

    ForeignToplevelHandleV3 *createHandle(Resource *managerResource, SurfaceEntry *entry);
    void setupHandleForWrapper(SurfaceEntry *entry, ForeignToplevelHandleV3 *handle);
    ForeignToplevelHandleV3 *findHandleForClient(SurfaceWrapper *wrapper, wl_client *client);

    void releaseHandle(ForeignToplevelHandleV3 *handle);
protected:
    void destroy(Resource *resource) override;
    void bind_resource(Resource *resource) override;
    void stop(Resource *resource) override;
    void get_dock_preview_context(Resource *resource, struct ::wl_resource *relative_surface, uint32_t id) override;
};

ForeignToplevelManagerInterfaceV3Private::ForeignToplevelManagerInterfaceV3Private(ForeignToplevelManagerInterfaceV3 *_q)
    : QtWaylandServer::treeland_foreign_toplevel_manager_v3()
    , q(_q)
{
}

wl_global *ForeignToplevelManagerInterfaceV3Private::global() const
{
    return m_global;
}

void ForeignToplevelManagerInterfaceV3Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ForeignToplevelManagerInterfaceV3Private::stop(Resource *resource)
{
    send_finished(resource->handle);
}

void ForeignToplevelManagerInterfaceV3Private::get_dock_preview_context(Resource *resource,
                                                                        struct ::wl_resource *relative_surface,
                                                                        uint32_t id)
{
    if (!relative_surface) {
        wl_resource_post_error(resource->handle,
                               TREELAND_FOREIGN_TOPLEVEL_MANAGER_V3_ERROR_INVALID_SURFACE,
                               "relative_surface resource is NULL!");
        return;
    }

    if (wl_resource_get_client(relative_surface) != resource->client()) {
        wl_resource_post_error(resource->handle,
                               TREELAND_FOREIGN_TOPLEVEL_MANAGER_V3_ERROR_INVALID_SURFACE,
                               "relative_surface is not owned by the calling client");
        return;
    }

    auto *relativeSurface = wlr_surface_from_resource(relative_surface);
    if (!relativeSurface) {
        wl_resource_post_error(resource->handle,
                               TREELAND_FOREIGN_TOPLEVEL_MANAGER_V3_ERROR_INVALID_SURFACE,
                               "wlr_surface_from_resource failed!");
        return;
    }

    // v3 protocol: only one dock preview context may be active per surface at a
    // time. If the surface already has an active context, the compositor raises
    // the context_already_exists protocol error and does not create the object.
    if (auto *relativeSurfaceObject = WSurface::fromHandle(relativeSurface)) {
        auto existing = relativeSurfaceObject->property(DockPreviewContextPropertyName).value<QObject *>();
        if (qobject_cast<DockPreviewContextV3 *>(existing)) {
            wl_resource_post_error(resource->handle,
                                   TREELAND_FOREIGN_TOPLEVEL_MANAGER_V3_ERROR_CONTEXT_ALREADY_EXISTS,
                                   "surface already has an active dock preview context; destroy it first");
            return;
        }
    }

    wl_resource *dockPreviewContextResource = wl_resource_create(resource->client(),
                                                                 &treeland_dock_preview_context_v3_interface,
                                                                 resource->version(),
                                                                 id);
    if (!dockPreviewContextResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *context = new DockPreviewContextV3(dockPreviewContextResource,
                                             relativeSurface,
                                             q);
    dockPreviewContexts.append(context);

    QObject::connect(context,
                     &DockPreviewContextV3::requestShow,
                     q,
                     [this, context](const QPoint &pos,
                                     ForeignToplevelManagerInterfaceV3::PreviewDirection direction,
                                     const QList<ForeignToplevelHandleV3 *> &toplevels) {
                         std::vector<SurfaceWrapper *> surfaces;
                         surfaces.reserve(toplevels.size());
                         for (auto *handle : toplevels) {
                             if (auto *entry = handle->entry()) {
                                 surfaces.push_back(entry->wrapper);
                             }
                         }
                         Q_EMIT q->requestDockPreview(
                             surfaces, context->relativeSurface(), pos, direction);
                     });
    QObject::connect(context,
                     &DockPreviewContextV3::requestShowTooltip,
                     q,
                     [this, context](WSurface *surface,
                                     const QPoint &pos,
                                     ForeignToplevelManagerInterfaceV3::PreviewDirection direction) {
                         Q_EMIT q->requestDockPreviewTooltip(surface,
                                                             context->relativeSurface(),
                                                             pos,
                                                             direction);
                     });
    QObject::connect(context,
                     &DockPreviewContextV3::requestHideTooltip,
                     q,
                     &ForeignToplevelManagerInterfaceV3::requestDockHideTooltip);
    QObject::connect(context,
                     &DockPreviewContextV3::requestClose,
                     q,
                     &ForeignToplevelManagerInterfaceV3::requestDockClose);
    QObject::connect(context,
                     &DockPreviewContextV3::beforeDestroy,
                     q,
                     &ForeignToplevelManagerInterfaceV3::requestDockClose);

    if (auto *relativeSurfaceObject = WSurface::fromHandle(relativeSurface)) {
        relativeSurfaceObject->setProperty(DockPreviewContextPropertyName,
                                           QVariant::fromValue(static_cast<QObject *>(context)));
    }
}

ForeignToplevelManagerInterfaceV3::ForeignToplevelManagerInterfaceV3(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new ForeignToplevelManagerInterfaceV3Private(this))
{
}

ForeignToplevelManagerInterfaceV3::~ForeignToplevelManagerInterfaceV3() = default;

ForeignToplevelHandleV3 *
ForeignToplevelManagerInterfaceV3Private::createHandle(Resource *managerResource, SurfaceEntry *entry)
{
    struct wl_client *client = wl_resource_get_client(managerResource->handle);
    struct wl_resource *resource = wl_resource_create(client,
                                                      &treeland_foreign_toplevel_handle_v3_interface,
                                                      wl_resource_get_version(managerResource->handle),
                                                      0);
    if (!resource) {
        qCCritical(lcTlProtocol) << "wl_resource_create failed!";
        wl_client_post_no_memory(client);
        return nullptr;
    }

    auto *handle = new ForeignToplevelHandleV3(q, resource, entry);
    entry->handles.append(handle);
    handles.append(handle);

    send_toplevel(managerResource->handle, resource);
    handle->set_identifier(m_nextIdentifier++);

    return handle;
}

void ForeignToplevelManagerInterfaceV3Private::setupHandleForWrapper(SurfaceEntry *entry,
                                                                      ForeignToplevelHandleV3 *handle)
{
    SurfaceWrapper *wrapper = entry->wrapper;

    QObject::connect(handle, &ForeignToplevelHandleV3::requestClose, wrapper, [wrapper]() {
        wrapper->close();
    });

    // The icon geometry handler only depends on the wrapper, so connect it here for
    // both splash and normal handles: a splash is exposed to clients immediately
    // and a set_icon_geometry sent during the splash phase must not be dropped.
    QObject::connect(handle,
            &ForeignToplevelHandleV3::iconGeometryChanged,
            wrapper,
            [wrapper](WSurface *surface, const QRect &rect) {
                         auto *dockWrapper =
                             WaylandServer::instance()->rootSurfaceContainer()->getSurface(surface);
                         if (!dockWrapper) {
                             qCWarning(lcTlProtocol)
                                 << "iconGeometryChanged: dock wrapper not found for app"
                                 << wrapper->appId() << "surface=" << surface << "rect=" << rect;
                             return;
                         }
                         const QRect iconGeometry(dockWrapper->x() + rect.x(),
                                                  dockWrapper->y() + rect.y(),
                                                  rect.width(),
                                                  rect.height());
                         qCDebug(lcTlProtocol) << "iconGeometryChanged:" << wrapper->appId()
                                               << "rect=" << rect
                                               << "dockPos="
                                               << QPointF(dockWrapper->x(), dockWrapper->y())
                                               << "iconGeometry=" << iconGeometry;
                         wrapper->setIconGeometry(iconGeometry);
                     });

    if (wrapper->type() == SurfaceWrapper::Type::SplashScreen) {
        handle->set_title(QStringLiteral("SplashScreen: ") + wrapper->appId());
        handle->set_app_id(wrapper->appId());
        handle->set_minimized(false);
        handle->set_maximized(false);
        handle->set_fullscreen(false);
        handle->set_activated(false);

        QObject::connect(wrapper, &SurfaceWrapper::surfaceItemCreated, handle, [this, wrapper, handle]() {
            q->initializeToplevelHandle(wrapper, handle);
        });
        return;
    }

    q->initializeToplevelHandle(wrapper, handle);
}

ForeignToplevelHandleV3 *
ForeignToplevelManagerInterfaceV3Private::findHandleForClient(SurfaceWrapper *wrapper, wl_client *client)
{
    auto it = m_surfaces.find(wrapper);
    if (it == m_surfaces.end()) {
        return nullptr;
    }

    for (auto *handle : std::as_const(it->second->handles)) {
        if (wl_resource_get_client(handle->resource()) == client) {
            return handle;
        }
    }

    return nullptr;
}

void ForeignToplevelManagerInterfaceV3::addSurface(SurfaceWrapper *wrapper)
{
    if (d->m_surfaces.contains(wrapper)) {
        qCCritical(lcTlProtocol)
        << wrapper << " has been add to foreign toplevel twice";
        return;
    }

    auto entry = std::make_unique<SurfaceEntry>();
    entry->wrapper = wrapper;
    auto *entryPtr = entry.get();
    d->m_surfaces.insert({wrapper, std::move(entry)});

    for (const auto &manager_resource : d->resourceMap()) {
        auto *handle = d->createHandle(manager_resource, entryPtr);
        if (!handle) {
            break;
        }
        d->setupHandleForWrapper(entryPtr, handle);
    }
}

void ForeignToplevelManagerInterfaceV3::removeSurface(SurfaceWrapper *wrapper)
{
    auto it = d->m_surfaces.find(wrapper);
    if (it == d->m_surfaces.end()) {
        qCCritical(lcTlProtocol) << wrapper << " is not registered in foreign toplevel";
        return;
    }

    auto *entry = it->second.get();
    auto *surface = wrapper->shellSurface();
    for (auto *handle : std::as_const(entry->handles)) {
        if (surface) {
            QObject::disconnect(surface, nullptr, handle, nullptr);
            if (auto *wsurface = surface->surface()) {
                QObject::disconnect(wsurface, nullptr, handle, nullptr);
            }
        }
        QObject::disconnect(wrapper, nullptr, handle, nullptr);
        QObject::disconnect(handle, nullptr, wrapper, nullptr);

        handle->send_closed();
        handle->clearEntry();
    }

    d->m_surfaces.erase(it);
}

void ForeignToplevelManagerInterfaceV3::releaseHandle(ForeignToplevelHandleV3 *handle)
{
    d->handles.removeOne(handle);
    d->releaseHandle(handle);
}

void ForeignToplevelManagerInterfaceV3::releaseDockPreviewContext(DockPreviewContextV3 *context)
{
    d->dockPreviewContexts.removeOne(context);
}

ForeignToplevelHandleV3 *ForeignToplevelManagerInterfaceV3::handleForIdentifier(uint32_t identifier) const
{
    for (auto *handle : std::as_const(d->handles)) {
        if (handle->identifier() == identifier) {
            return handle;
        }
    }
    return nullptr;
}

void ForeignToplevelManagerInterfaceV3Private::releaseHandle(ForeignToplevelHandleV3 *handle)
{
    auto *entry = handle->entry();
    if (entry) {
        entry->handles.removeOne(handle);
    }
}

void ForeignToplevelManagerInterfaceV3Private::bind_resource(Resource *resource)
{
    for (auto &[wrapper, entry] : m_surfaces) {
        auto *handle = createHandle(resource, entry.get());
        if (!handle) {
            return;
        }
        setupHandleForWrapper(entry.get(), handle);
    }
}

void ForeignToplevelManagerInterfaceV3::enterDockPreview(WSurface *relativeSurface)
{
    if (auto *context = dockPreviewContextForSurface(relativeSurface)) {
        context->enter();
    }
}

void ForeignToplevelManagerInterfaceV3::leaveDockPreview(WSurface *relativeSurface)
{
    if (auto *context = dockPreviewContextForSurface(relativeSurface)) {
        context->leave();
    }
}

wl_event_loop *ForeignToplevelManagerInterfaceV3::eventLoop() const
{
    return d->event_loop;
}

QByteArrayView ForeignToplevelManagerInterfaceV3::interfaceName() const
{
    return d->interfaceName();
}

void ForeignToplevelManagerInterfaceV3::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
    d->event_loop = wl_display_get_event_loop(server->handle());
}

void ForeignToplevelManagerInterfaceV3::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *ForeignToplevelManagerInterfaceV3::global() const
{
    return d->global();
}

void ForeignToplevelManagerInterfaceV3::initializeToplevelHandle(SurfaceWrapper *wrapper, ForeignToplevelHandleV3 *handle)
{
    Q_ASSERT(wrapper->type() == SurfaceWrapper::Type::XdgToplevel
             || wrapper->type() == SurfaceWrapper::Type::XWayland);
    auto surface = wrapper->shellSurface();
    qCInfo(lcTlProtocol) << "Register surface to ForeignToplevelManagerInterfaceV3, appId=" << wrapper->appId()
                             << wrapper->type() << wrapper->skipDockPreView();

    // initSurface
    handle->set_title(surface->title());
    handle->set_app_id(wrapper->appId());
    handle->set_minimized(surface->isMinimized());
    handle->set_maximized(surface->isMaximized());
    handle->set_fullscreen(surface->isFullScreen());
    handle->set_activated(surface->isActivated());
    handle->set_attention(wrapper->attention());

    QObject::connect(surface, &WToplevelSurface::titleChanged, handle, [handle, surface] {
        handle->set_title(surface->title());
    });

    QObject::connect(surface, &WToplevelSurface::minimizeChanged, handle, [handle, surface] {
        handle->set_minimized(surface->isMinimized());
    });

    QObject::connect(surface, &WToplevelSurface::maximizeChanged, handle, [handle, surface] {
        handle->set_maximized(surface->isMaximized());
    });

    QObject::connect(surface, &WToplevelSurface::fullscreenChanged, handle, [handle, surface] {
        handle->set_fullscreen(surface->isFullScreen());
    });

    QObject::connect(surface, &WToplevelSurface::activateChanged, handle, [handle, surface] {
        handle->set_activated(surface->isActivated());
    });

    connect(wrapper, &SurfaceWrapper::attentionChanged, handle, [handle, wrapper] {
        handle->set_attention(wrapper->attention());
    });

    QObject::connect(surface, &WToplevelSurface::appIdChanged, handle, [handle, wrapper] {
        handle->set_app_id(wrapper->appId());
    });

    QObject::connect(surface->surface(), &WSurface::outputEntered, handle, [handle](WOutput *output) {
        handle->output_enter(output);
    });

    QObject::connect(surface->surface(), &WSurface::outputLeave, handle, [handle](WOutput *output) {
        handle->output_leave(output);
    });

    connect(handle, &ForeignToplevelHandleV3::requestActivate, wrapper, [wrapper](WSeat *seat) {
        WaylandServer::instance()->forceActivateSurface(wrapper, Qt::OtherFocusReason, seat);
    });

    connect(handle,
            &ForeignToplevelHandleV3::requestMaximize,
            wrapper,
            [wrapper](bool maximized) {
                if (maximized)
                    wrapper->maximize();
                else
                    wrapper->unmaximize();
            });

    connect(handle,
            &ForeignToplevelHandleV3::requestMinimize,
            wrapper,
            [wrapper](bool minimized) {
                if ((WaylandServer::instance()->showDesktopState()
                     == ShowDesktopInterfaceV1::State::Show)) {
                    WaylandServer::instance()->forceActivateSurface(wrapper);
                    return;
                }

                if (minimized)
                    wrapper->minimize();
                else
                    wrapper->restoreFromMinimized();
            });

    connect(handle,
            &ForeignToplevelHandleV3::requestFullscreen,
            wrapper,
            [wrapper](bool fullscreen, WOutput *output) {
                if (fullscreen)
                    wrapper->enterFullscreen(output);
                else
                    wrapper->leaveFullscreen();
            });

    if (auto *xdgSurface = qobject_cast<WXdgToplevelSurface *>(surface)) {
        auto client = WClient::get(xdgSurface->handle()->resource->client);
        handle->set_pid(client->credentials().get()->pid);

        auto updateSurfaceParent = [this, handle, xdgSurface] {
            WXdgToplevelSurface *p = xdgSurface->parentXdgSurface();
            if (!p) {
                handle->set_parent(nullptr);
                return;
            }
            wl_client *client = wl_resource_get_client(handle->resource());
            for (const auto &[wrapper, entry] : d->m_surfaces) {
                if (wrapper->shellSurface() == p) {
                    auto *parentHandle = d->findHandleForClient(wrapper, client);
                    if (parentHandle) {
                        handle->set_parent(parentHandle);
                    }
                    return;
                }
            }
            qCCritical(lcTlProtocol)
                << "Xdg toplevel surface " << xdgSurface
                << "has set parent surface, but foreign_toplevel_handle for "
                   "parent surface not "
                   "found!";
        };
        QObject::connect(xdgSurface, &WXdgToplevelSurface::parentXdgSurfaceChanged,
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
            wl_client *client = wl_resource_get_client(handle->resource());
            for (const auto &[wrapper, entry] : d->m_surfaces) {
                if (wrapper->shellSurface() == p) {
                    auto *parentHandle = d->findHandleForClient(wrapper, client);
                    if (parentHandle) {
                        handle->set_parent(parentHandle);
                    }
                    return;
                }
            }
            qCCritical(lcTlProtocol)
                << "X11 surface " << xwaylandSurface
                << "has set parent surface, but foreign_toplevel_handle for "
                   "parent surface not "
                   "found!";
        };
        QObject::connect(xwaylandSurface, &WXWaylandSurface::parentXWaylandSurfaceChanged,
                                     handle,
                                     updateSurfaceParent);
        updateSurfaceParent();
    } else {
        qCFatal(lcTlProtocol)
        << "TreelandForeignToplevelManager only support WXdgSurface or "
           "WXWaylandSurface";
    }
}

namespace {
// A dedicated role keeps the tooltip surface away from every other surface
// user: wlroots refuses to assign another role while this one is set, and never
// clears it, so a surface that once showed a tooltip can only show tooltips.
const struct wlr_surface_role dockTooltipSurfaceRole = {
    "treeland_dock_tooltip",
    true,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};
}

class DockPreviewContextV3Private
    : public QtWaylandServer::treeland_dock_preview_context_v3
{
public:
    DockPreviewContextV3Private(DockPreviewContextV3 *_q,
                                wl_resource *resource,
                                wlr_surface *_relativeSurface,
                                ForeignToplevelManagerInterfaceV3 *_manager);
    ~DockPreviewContextV3Private() override;

    DockPreviewContextV3 *q = nullptr;
    QPointer<ForeignToplevelManagerInterfaceV3> manager;
    QPointer<WSurface> relativeSurface;

    // The surface this context currently shows as its tooltip, if any. The
    // surface belongs to the client; the wrapper is ours when nobody else had
    // wrapped that surface yet.
    QPointer<WSurface> tooltipSurface;
    // Whether the tooltip, and not the preview, is what the UI displays.
    bool tooltipShown = false;

    void setTooltipSurface(wlr_surface *surface);
    void releaseTooltipSurface();
    void hideTooltip();
    void enterTooltipOutputs();
    void leaveTooltipOutputs();

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void show(Resource *resource, wl_array *identifiers, int32_t x, int32_t y, uint32_t direction) override;
    void show_tooltip_surface(Resource *resource, struct ::wl_resource *surface, int32_t x, int32_t y, uint32_t direction) override;
    void close(Resource *resource) override;

private:
    static void tooltipSurfaceDestroyed(wl_listener *listener, void *data);

    wl_listener m_tooltipDestroyListener{};
    bool m_tooltipListening = false;
    bool m_ownsTooltipWrapper = false;
};

DockPreviewContextV3Private::DockPreviewContextV3Private(DockPreviewContextV3 *_q,
                                                         wl_resource *resource,
                                                         wlr_surface *_relativeSurface,
                                                         ForeignToplevelManagerInterfaceV3 *_manager)
    : QtWaylandServer::treeland_dock_preview_context_v3(resource)
    , q(_q)
    , manager(_manager)
    , relativeSurface(WSurface::fromHandle(_relativeSurface))
{
}

DockPreviewContextV3Private::~DockPreviewContextV3Private()
{
    releaseTooltipSurface();
}

void DockPreviewContextV3Private::setTooltipSurface(wlr_surface *surface)
{
    if (tooltipSurface && tooltipSurface->handle() == surface)
        return;

    releaseTooltipSurface();

    auto *wrapper = WSurface::fromHandle(surface);
    if (wrapper) {
        m_ownsTooltipWrapper = false;
    } else {
        wrapper = new WSurface(surface);
        m_ownsTooltipWrapper = true;
    }
    tooltipSurface = wrapper;

    m_tooltipDestroyListener.notify = &DockPreviewContextV3Private::tooltipSurfaceDestroyed;
    wl_signal_add(&surface->events.destroy, &m_tooltipDestroyListener);
    m_tooltipListening = true;
}

void DockPreviewContextV3Private::releaseTooltipSurface()
{
    leaveTooltipOutputs();

    if (m_tooltipListening) {
        wl_list_remove(&m_tooltipDestroyListener.link);
        m_tooltipListening = false;
    }

    if (m_ownsTooltipWrapper) {
        m_ownsTooltipWrapper = false;
        // Emits WSurface::beforeDestroy, so the UI drops the pointer before
        // the wrapper goes away.
        delete tooltipSurface.data();
    }
    tooltipSurface = nullptr;
}

// The compositor renders this surface, so the client has to be told which
// output it is shown on, like any other surface: that is what carries the
// preferred scale and the frame callbacks.
void DockPreviewContextV3Private::enterTooltipOutputs()
{
    if (!tooltipSurface || !relativeSurface)
        return;

    for (auto *output : relativeSurface->outputs())
        tooltipSurface->enterOutput(output);
}

void DockPreviewContextV3Private::leaveTooltipOutputs()
{
    if (!tooltipSurface)
        return;

    const auto outputs = tooltipSurface->outputs();
    for (auto *output : outputs)
        tooltipSurface->leaveOutput(output);
}

void DockPreviewContextV3Private::hideTooltip()
{
    leaveTooltipOutputs();

    if (!tooltipShown)
        return;

    tooltipShown = false;
    Q_EMIT q->requestHideTooltip();
}

void DockPreviewContextV3Private::tooltipSurfaceDestroyed(wl_listener *listener,
                                                          [[maybe_unused]] void *data)
{
    DockPreviewContextV3Private *context = nullptr;
    context = wl_container_of(listener, context, m_tooltipDestroyListener);

    wl_list_remove(&context->m_tooltipDestroyListener.link);
    context->m_tooltipListening = false;

    // The wrapper of a surface the client destroyed must go with it.
    if (context->m_ownsTooltipWrapper) {
        context->m_ownsTooltipWrapper = false;
        delete context->tooltipSurface.data();
    }
    context->tooltipSurface = nullptr;

    context->hideTooltip();
}

void DockPreviewContextV3Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    releaseTooltipSurface();
    if (relativeSurface) {
        relativeSurface->setProperty(DockPreviewContextPropertyName, QVariant());
    }
    if (manager) {
        manager->releaseDockPreviewContext(q);
    }
    Q_EMIT q->beforeDestroy();
    delete q;
}

void DockPreviewContextV3Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DockPreviewContextV3Private::show([[maybe_unused]] Resource *resource, wl_array *identifiers, int32_t x, int32_t y, uint32_t direction)
{
    if (!relativeSurface) {
        return;
    }

    QList<ForeignToplevelHandleV3 *> toplevels;
    const uint32_t *data = reinterpret_cast<const uint32_t *>(identifiers->data);
    const size_t count = identifiers->size / sizeof(uint32_t);
    for (size_t i = 0; i != count; ++i) {
        if (auto *handle = manager ? manager->handleForIdentifier(data[i]) : nullptr) {
            toplevels.append(handle);
        }
    }

    if (!identifiers->size)
        qCCritical(lcTlProtocol) << "Got empty identifier list for dock preview!";

    // The preview replaces the tooltip.
    tooltipShown = false;
    leaveTooltipOutputs();

    Q_EMIT q->requestShow(QPoint(x, y),
                          static_cast<ForeignToplevelManagerInterfaceV3::PreviewDirection>(direction),
                          toplevels);
}

void DockPreviewContextV3Private::show_tooltip_surface(Resource *resource,
                                                       struct ::wl_resource *surfaceResource,
                                                       int32_t x,
                                                       int32_t y,
                                                       uint32_t direction)
{
    if (!relativeSurface)
        return;

    auto *surface = wlr_surface_from_resource(surfaceResource);
    if (wl_resource_get_client(surfaceResource) != resource->client()
        || surface == relativeSurface->handle()) {
        wl_resource_post_error(resource->handle,
                               error_invalid_surface,
                               "tooltip surface must belong to the calling client and must not be the anchor surface");
        return;
    }

    // A surface without committed content is a client error, and the request
    // has no effect.
    if (!wlr_surface_get_texture(surface))
        return;

    // Fails and posts the protocol error itself when the surface already has
    // another role.
    if (!wlr_surface_set_role(surface,
                              &dockTooltipSurfaceRole,
                              resource->handle,
                              error_invalid_surface)) {
        return;
    }

    setTooltipSurface(surface);

    tooltipShown = true;
    enterTooltipOutputs();
    Q_EMIT q->requestShowTooltip(
        tooltipSurface.data(),
        QPoint(x, y),
        static_cast<ForeignToplevelManagerInterfaceV3::PreviewDirection>(direction));
}

void DockPreviewContextV3Private::close([[maybe_unused]] Resource *resource)
{
    tooltipShown = false;
    leaveTooltipOutputs();
    Q_EMIT q->requestClose();
}

DockPreviewContextV3::~DockPreviewContextV3() = default;

wl_resource *DockPreviewContextV3::resource() const
{
    return d->resource()->handle;
}

WSurface *DockPreviewContextV3::relativeSurface() const
{
    return d->relativeSurface;
}

void DockPreviewContextV3::enter()
{
    d->send_enter();
}

void DockPreviewContextV3::leave()
{
    d->send_leave();
}

DockPreviewContextV3::DockPreviewContextV3(wl_resource *resource,
                                           wlr_surface *_relativeSurface,
                                           ForeignToplevelManagerInterfaceV3 *manager)
    : QObject(nullptr)
    , d(new DockPreviewContextV3Private(this, resource, _relativeSurface, manager))
{
}

class ForeignToplevelHandleV3Private
    : public QtWaylandServer::treeland_foreign_toplevel_handle_v3
{
public:
    ForeignToplevelHandleV3Private(ForeignToplevelHandleV3 *_q,
                                   ForeignToplevelManagerInterfaceV3 *_manager,
                                   wl_resource *resource,
                                   SurfaceEntry *_entry);
    ~ForeignToplevelHandleV3Private() override;

    ForeignToplevelHandleV3 *q = nullptr;
    QPointer<ForeignToplevelManagerInterfaceV3> manager;
    SurfaceEntry *entry = nullptr;

    wl_event_source *idle_source{ nullptr };

    QString title;
    QString app_id;
    uint32_t identifier;
    pid_t pid;

    ForeignToplevelHandleV3 *parent{ nullptr };
    QList<foreign_toplevel_output> outputs;
    ForeignToplevelHandleV3::States state;
    void scheduleDone();
    static void idleSendDone(void *data);
protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_maximized(Resource *resource) override;
    void unset_maximized(Resource *resource) override;
    void set_minimized(Resource *resource) override;
    void unset_minimized(Resource *resource) override;
    void activate(Resource *resource, struct ::wl_resource *seat) override;
    void close(Resource *resource) override;
    void set_icon_geometry(Resource *resource, struct ::wl_resource *surface, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void set_fullscreen(Resource *resource, struct ::wl_resource *output) override;
    void unset_fullscreen(Resource *resource) override;
};

ForeignToplevelHandleV3Private::ForeignToplevelHandleV3Private(ForeignToplevelHandleV3 *_q,
                                                               ForeignToplevelManagerInterfaceV3 *_manager,
                                                               wl_resource *resource,
                                                               SurfaceEntry *_entry)
    : QtWaylandServer::treeland_foreign_toplevel_handle_v3(resource)
    , q(_q)
    , manager(_manager)
    , entry(_entry)
{
}

ForeignToplevelHandleV3Private::~ForeignToplevelHandleV3Private()
{
    if (idle_source) {
        wl_event_source_remove(idle_source);
        idle_source = nullptr;
    }
    send_done();
}

void ForeignToplevelHandleV3Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    if (manager) {
        manager->releaseHandle(q);
    }

    delete q;
}

void ForeignToplevelHandleV3Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ForeignToplevelHandleV3Private::set_maximized([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestMaximize(true);
}

void ForeignToplevelHandleV3Private::unset_maximized([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestMaximize(false);
}

void ForeignToplevelHandleV3Private::set_minimized([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestMinimize(true);
}

void ForeignToplevelHandleV3Private::unset_minimized([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestMinimize(false);
}

void ForeignToplevelHandleV3Private::activate(Resource *resource, struct ::wl_resource *seat)
{
    const wlr_seat_client *seat_client = wlr_seat_client_from_resource(seat);
    if (!seat_client) {
        wl_resource_post_error(resource->handle, 0, "wlr_seat_client_from_resource failed!");
        return;
    }

    Q_EMIT q->requestActivate(WSeat::fromHandle(seat_client->seat));
}

void ForeignToplevelHandleV3Private::close([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestClose();
}

void ForeignToplevelHandleV3Private::set_icon_geometry(Resource *resource, struct ::wl_resource *surface, int32_t x, int32_t y, int32_t width, int32_t height)
{
    if (width < 0 || height < 0) {
        wl_resource_post_error(resource->handle,
                               TREELAND_FOREIGN_TOPLEVEL_HANDLE_V3_ERROR_INVALID_GEOMETRY,
                               "invalid geometry passed to set_icon_geometry: width/height < 0");
        return;
    }

    auto *wlrSurface = wlr_surface_from_resource(surface);
    auto *surfaceObject = wlrSurface ? WSurface::fromHandle(wlrSurface) : nullptr;
    Q_EMIT q->iconGeometryChanged(surfaceObject, QRect(x, y, width, height));
}

void ForeignToplevelHandleV3Private::set_fullscreen(Resource *resource, struct ::wl_resource *output)
{
    WOutput *wrappedOutput = nullptr;
    if (output) {
        auto *wlrOutput = wlr_output_from_resource(output);

        if (!wlrOutput) {
            wl_resource_post_error(resource->handle, 0, "wlr_output_from_resource failed!");
            return;
        }
        wrappedOutput = WOutput::fromHandle(wlrOutput);
    }

    Q_EMIT q->requestFullscreen(true, wrappedOutput);
}

void ForeignToplevelHandleV3Private::unset_fullscreen([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestFullscreen(false, nullptr);
}

ForeignToplevelHandleV3::~ForeignToplevelHandleV3()
{
    // Detaches bind listeners registered via output->listeners(this).
    teardown();
}

wl_resource *ForeignToplevelHandleV3::resource() const
{
    return d->resource()->handle;
}

void ForeignToplevelHandleV3::set_title(const QString &title)
{
    if (d->title == title) {
        return;
    }

    d->title = title;
    d->send_title(title);
    d->scheduleDone();
}

void ForeignToplevelHandleV3::set_app_id(const QString &app_id)
{
    if (d->app_id == app_id) {
        return;
    }

    d->app_id = app_id;
    d->send_app_id(app_id);
    d->scheduleDone();
}

void ForeignToplevelHandleV3::set_pid(const pid_t pid)
{
    d->pid = pid;
    d->send_pid(pid);
    d->scheduleDone();
}

void ForeignToplevelHandleV3::set_identifier(uint32_t identifier)
{
    d->identifier = identifier;
    d->send_identifier(identifier);
    d->scheduleDone();
}

uint32_t ForeignToplevelHandleV3::identifier() const
{
    return d->identifier;
}

void ForeignToplevelHandleV3::output_enter(WOutput *output)
{
    if (!output) {
        return;
    }

    auto *wlrOutput = output->handle();
    if (std::any_of(d->outputs.begin(),
                    d->outputs.end(),
                    [output](const foreign_toplevel_output &toplevel_output) {
                        return toplevel_output.output == output;
                    }))
        return;

    auto toplevel_output = foreign_toplevel_output{ .output = output, .toplevel = this };
    d->outputs.append(toplevel_output);

    output->listeners(this)->add(&wlrOutput->events.bind, this,
        [toplevel_output] (wlr_output_event_bind *event) {
        const wl_client *client = wl_resource_get_client(event->resource);
        if (wl_resource_get_client(toplevel_output.toplevel->resource()) == client) {
            toplevel_output.toplevel->send_output(toplevel_output.output, true);
        }
    });

    send_output(output, true);
}

void ForeignToplevelHandleV3::output_leave(WOutput *output)
{
    if (!output) {
        return;
    }

    // Detach the bind listener registered via output->listeners(this).
    output->removeListeners(this);
    d->outputs.removeIf([output](const foreign_toplevel_output &handle_output) {
        return handle_output.output == output;
    });

    send_output(output, false);
}

void ForeignToplevelHandleV3::set_maximized(bool maximized)
{
    if (d->state.testFlag(State::Maximized) == maximized) {
        return;
    }
    d->state.setFlag(State::Maximized, maximized);
    send_state();
}

void ForeignToplevelHandleV3::set_minimized(bool minimized)
{
    if (d->state.testFlag(State::Minimized) == minimized) {
        return;
    }
    d->state.setFlag(State::Minimized, minimized);
    send_state();
}

void ForeignToplevelHandleV3::set_activated(bool activated)
{
    if (d->state.testFlag(State::Activated) == activated) {
        return;
    }
    d->state.setFlag(State::Activated, activated);
    send_state();
}

void ForeignToplevelHandleV3::set_fullscreen(bool fullscreen)
{
    if (d->state.testFlag(State::Fullscreen) == fullscreen) {
        return;
    }
    d->state.setFlag(State::Fullscreen, fullscreen);
    send_state();
}

void ForeignToplevelHandleV3::set_attention(bool attention)
{
    if (d->state.testFlag(State::Attention) == attention) {
        return;
    }
    d->state.setFlag(State::Attention, attention);
    send_state();
}

void ForeignToplevelHandleV3::set_parent(ForeignToplevelHandleV3 *parent)
{
    if (d->parent == parent) {
        return;
    }

    d->send_parent(parent ? parent->resource() : nullptr);
    d->parent = parent;
    d->scheduleDone();
}

void ForeignToplevelHandleV3::send_done()
{
    d->send_done();
}

void ForeignToplevelHandleV3::send_closed()
{
    d->send_closed();
}

void ForeignToplevelHandleV3::send_state()
{
    d->send_state(encodeStates(d->state));

    d->scheduleDone();
}

void ForeignToplevelHandleV3::send_output(WOutput *output, bool enter)
{
    if (!output) {
        return;
    }

    const wl_client *client = wl_resource_get_client(resource());
    struct wl_resource *output_resource;

    wl_resource_for_each(output_resource, &output->handle()->resources)
    {
        if (wl_resource_get_client(output_resource) == client) {
            if (enter) {
                treeland_foreign_toplevel_handle_v3_send_output_enter(resource(), output_resource);
            } else {
                treeland_foreign_toplevel_handle_v3_send_output_leave(resource(), output_resource);
            }
        }
    }

    d->scheduleDone();
}

SurfaceEntry *ForeignToplevelHandleV3::entry() const
{
    return d->entry;
}

void ForeignToplevelHandleV3::clearEntry()
{
    d->entry = nullptr;
}

ForeignToplevelHandleV3::ForeignToplevelHandleV3(ForeignToplevelManagerInterfaceV3 *manager,
                                                  wl_resource *resource,
                                                  SurfaceEntry *entry)
    : QObject(nullptr)
    , WObject()
    , d(new ForeignToplevelHandleV3Private(this, manager, resource, entry))
{
}

void ForeignToplevelHandleV3Private::idleSendDone(void *data)
{
    auto *self = static_cast<ForeignToplevelHandleV3Private *>(data);
    self->send_done();
    self->idle_source = nullptr;
}

void ForeignToplevelHandleV3Private::scheduleDone()
{
    if (idle_source || !manager) {
        return;
    }

    idle_source = wl_event_loop_add_idle(manager->eventLoop(), idleSendDone, this);
}
