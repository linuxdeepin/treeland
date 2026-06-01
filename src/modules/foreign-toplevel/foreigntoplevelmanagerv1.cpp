// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "foreigntoplevelmanagerv1.h"
#include "common/treelandlogging.h"
#include "core/rootsurfacecontainer.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"

#include "qwayland-server-treeland-foreign-toplevel-manager-v1.h"

#include <wayland-server.h>
#include <woutput.h>
#include <wsocket.h>
#include <wtoplevelsurface.h>
#include <wxdgtoplevelsurface.h>
#include <wxwaylandsurface.h>

#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwxdgshell.h>

static QList<DockPreviewContextV1 *> s_dockPreviewContexts;

struct SurfaceEntry
{
    SurfaceWrapper *wrapper = nullptr;
    QList<ForeignToplevelHandleV1 *> handles;
};

class ForeignToplevelManagerInterfaceV1Private
    : public QtWaylandServer::treeland_foreign_toplevel_manager_v1
{
public:
    ForeignToplevelManagerInterfaceV1Private(ForeignToplevelManagerInterfaceV1 *_q);
    ~ForeignToplevelManagerInterfaceV1Private() override = default;

    ForeignToplevelManagerInterfaceV1 *q = nullptr;
    wl_event_loop *event_loop = nullptr;

    wl_global *global() const;
    std::map<SurfaceWrapper *, std::unique_ptr<SurfaceEntry>> m_surfaces;
    uint32_t m_nextIdentifier = 1;

    ForeignToplevelHandleV1 *createHandle(Resource *managerResource, SurfaceEntry *entry);
    void setupHandleForWrapper(SurfaceEntry *entry, ForeignToplevelHandleV1 *handle);
    ForeignToplevelHandleV1 *findHandleForClient(SurfaceWrapper *wrapper, wl_client *client);

    void releaseHandle(ForeignToplevelHandleV1 *handle);
protected:
    void bind_resource(Resource *resource) override;
    void stop(Resource *resource) override;
    void get_dock_preview_context(Resource *resource, struct ::wl_resource *relative_surface, uint32_t id) override;
};

ForeignToplevelManagerInterfaceV1Private::ForeignToplevelManagerInterfaceV1Private(ForeignToplevelManagerInterfaceV1 *_q)
    : QtWaylandServer::treeland_foreign_toplevel_manager_v1()
    , q(_q)
{
}

wl_global *ForeignToplevelManagerInterfaceV1Private::global() const
{
    return m_global;
}

void ForeignToplevelManagerInterfaceV1Private::stop(Resource *resource)
{
    send_finished(resource->handle);
    wl_resource_destroy(resource->handle);
}

void ForeignToplevelManagerInterfaceV1Private::get_dock_preview_context(Resource *resource,
                                                                        struct ::wl_resource *relative_surface,
                                                                        uint32_t id)
{
    if (!relative_surface) {
        wl_resource_post_error(resource->handle, 0, "relative_surface resource is NULL!");
        return;
    }

    auto *relativeSurface = wlr_surface_from_resource(relative_surface);
    if (!relativeSurface) {
        wl_resource_post_error(resource->handle, 0, "wlr_surface_from_resource failed!");
        return;
    }

    wl_resource *dockPreviewContextResource = wl_resource_create(resource->client(),
                                                                 &treeland_dock_preview_context_v1_interface,
                                                                 resource->version(),
                                                                 id);
    if (!dockPreviewContextResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *context = new DockPreviewContextV1(dockPreviewContextResource,
                                             relativeSurface);
    s_dockPreviewContexts.append(context);
    QObject::connect(context, &QObject::destroyed, [context]() {
        s_dockPreviewContexts.removeOne(context);
    });

    QObject::connect(context,
                     &DockPreviewContextV1::requestShow,
                     q,
                     &ForeignToplevelManagerInterfaceV1::handleDockPreviewShow);
    QObject::connect(context,
                     &DockPreviewContextV1::requestShowTooltip,
                     q,
                     &ForeignToplevelManagerInterfaceV1::handleDockPreviewShowTooltip);
    QObject::connect(context,
                &DockPreviewContextV1::requestClose,
                q,
                &ForeignToplevelManagerInterfaceV1::requestDockClose);
    QObject::connect(context,
                &DockPreviewContextV1::beforeDestroy,
                q,
                &ForeignToplevelManagerInterfaceV1::requestDockClose);
}

ForeignToplevelManagerInterfaceV1::ForeignToplevelManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new ForeignToplevelManagerInterfaceV1Private(this))
{
}

ForeignToplevelManagerInterfaceV1::~ForeignToplevelManagerInterfaceV1() = default;

ForeignToplevelHandleV1 *
ForeignToplevelManagerInterfaceV1Private::createHandle(Resource *managerResource, SurfaceEntry *entry)
{
    struct wl_client *client = wl_resource_get_client(managerResource->handle);
    struct wl_resource *resource = wl_resource_create(client,
                                                      &treeland_foreign_toplevel_handle_v1_interface,
                                                      wl_resource_get_version(managerResource->handle),
                                                      0);
    if (!resource) {
        qCCritical(treelandProtocol) << "wl_resource_create failed!";
        wl_client_post_no_memory(client);
        return nullptr;
    }

    auto *handle = new ForeignToplevelHandleV1(q, resource, entry);
    entry->handles.append(handle);

    send_toplevel(managerResource->handle, resource);
    handle->set_identifier(m_nextIdentifier++);

    return handle;
}

void ForeignToplevelManagerInterfaceV1Private::setupHandleForWrapper(SurfaceEntry *entry,
                                                                      ForeignToplevelHandleV1 *handle)
{
    SurfaceWrapper *wrapper = entry->wrapper;

    QObject::connect(handle, &ForeignToplevelHandleV1::requestClose, wrapper, [wrapper]() {
        wrapper->close();
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

ForeignToplevelHandleV1 *
ForeignToplevelManagerInterfaceV1Private::findHandleForClient(SurfaceWrapper *wrapper, wl_client *client)
{
    auto it = m_surfaces.find(wrapper);
    if (it == m_surfaces.end()) {
        return nullptr;
    }

    for (auto *handle : it->second->handles) {
        if (wl_resource_get_client(handle->resource()) == client) {
            return handle;
        }
    }

    return nullptr;
}

void ForeignToplevelManagerInterfaceV1::addSurface(SurfaceWrapper *wrapper)
{
    if (d->m_surfaces.contains(wrapper)) {
        qCCritical(treelandProtocol)
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

void ForeignToplevelManagerInterfaceV1::removeSurface(SurfaceWrapper *wrapper)
{
    auto it = d->m_surfaces.find(wrapper);
    if (it == d->m_surfaces.end()) {
        qCCritical(treelandProtocol) << wrapper << " is not registered in foreign toplevel";
        return;
    }

    auto *entry = it->second.get();
    for (auto *handle : entry->handles) {
        handle->send_closed();
        handle->clearEntry();
    }

    d->m_surfaces.erase(it);
}

void ForeignToplevelManagerInterfaceV1::releaseHandle(ForeignToplevelHandleV1 *handle)
{
    d->releaseHandle(handle);
}

void ForeignToplevelManagerInterfaceV1Private::releaseHandle(ForeignToplevelHandleV1 *handle)
{
    auto *entry = handle->entry();
    if (entry) {
        entry->handles.removeOne(handle);
    }
}

void ForeignToplevelManagerInterfaceV1Private::bind_resource(Resource *resource)
{
    for (auto &[wrapper, entry] : m_surfaces) {
        auto *handle = createHandle(resource, entry.get());
        if (!handle) {
            return;
        }
        setupHandleForWrapper(entry.get(), handle);
    }
}

void ForeignToplevelManagerInterfaceV1::enterDockPreview(WSurface *relativeSurface)
{
    for (auto *context : s_dockPreviewContexts) {
        if (context->relativeSurface() == relativeSurface) {
            context->enter();
            break;
        }
    }
}

void ForeignToplevelManagerInterfaceV1::leaveDockPreview(WSurface *relativeSurface)
{
    for (auto *context : s_dockPreviewContexts) {
        if (context->relativeSurface() == relativeSurface) {
            context->leave();
            break;
        }
    }
}

wl_event_loop *ForeignToplevelManagerInterfaceV1::eventLoop() const
{
    return d->event_loop;
}

QByteArrayView ForeignToplevelManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

void ForeignToplevelManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
    d->event_loop = wl_display_get_event_loop(server->handle()->handle());
}

void ForeignToplevelManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *ForeignToplevelManagerInterfaceV1::global() const
{
    return d->global();
}

void ForeignToplevelManagerInterfaceV1::initializeToplevelHandle(SurfaceWrapper *wrapper, ForeignToplevelHandleV1 *handle)
{
    Q_ASSERT(wrapper->type() == SurfaceWrapper::Type::XdgToplevel
             || wrapper->type() == SurfaceWrapper::Type::XWayland);
    auto surface = wrapper->shellSurface();
    qCInfo(treelandProtocol) << "Register surface to ForeignToplevelManagerInterfaceV1, appId=" << wrapper->appId()
                             << wrapper->type() << wrapper->skipDockPreView();

    // initSurface
    handle->set_title(surface->title());
    handle->set_app_id(wrapper->appId());
    handle->set_minimized(surface->isMinimized());
    handle->set_maximized(surface->isMaximized());
    handle->set_fullscreen(surface->isFullScreen());
    handle->set_activated(surface->isActivated());
    handle->set_attention(wrapper->attention());

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

    connect(wrapper, &SurfaceWrapper::attentionChanged, handle, [handle, wrapper] {
        handle->set_attention(wrapper->attention());
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
            &ForeignToplevelHandleV1::requestActivate,
            wrapper,
            [wrapper]([[maybe_unused]] treeland_foreign_toplevel_handle_v1_activated_event *event) {
                Helper::instance()->forceActivateSurface(wrapper);
            });

    connect(handle,
            &ForeignToplevelHandleV1::requestMaximize,
            wrapper,
            [wrapper](treeland_foreign_toplevel_handle_v1_maximized_event *event) {
                if (event->maximized)
                    wrapper->requestMaximize();
                else
                    wrapper->requestCancelMaximize();
            });

    connect(handle,
            &ForeignToplevelHandleV1::requestMinimize,
            wrapper,
            [wrapper](treeland_foreign_toplevel_handle_v1_minimized_event *event) {
                if ((Helper::instance()->showDesktopState()
                     == WindowManagementInterfaceV1::DesktopState::Show)) {
                    Helper::instance()->forceActivateSurface(wrapper);
                    return;
                }

                if (event->minimized)
                    wrapper->requestMinimize();
                else
                    wrapper->requestCancelMinimize();
            });

    connect(handle,
            &ForeignToplevelHandleV1::requestFullscreen,
            wrapper,
            [wrapper](treeland_foreign_toplevel_handle_v1_fullscreen_event *event) {
                if (event->fullscreen)
                    wrapper->requestFullscreen();
                else
                    wrapper->requestCancelFullscreen();
            });

    connect(handle,
            &ForeignToplevelHandleV1::rectangleChanged,
            wrapper,
            [wrapper](treeland_foreign_toplevel_handle_v1_set_rectangle_event *event) {
                auto dockWrapper = Helper::instance()->rootSurfaceContainer()->getSurface(
                    WSurface::fromHandle(event->surface));
                if (!dockWrapper)
                    return;
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

void ForeignToplevelManagerInterfaceV1::handleDockPreviewShow(treeland_dock_preview_context_v1_preview_event *event)
{
    DockPreviewContextV1 *dockPreview = static_cast<DockPreviewContextV1 *>(sender());
    std::vector<SurfaceWrapper *> surfaces;
    for (auto toplevelIt = event->toplevels.cbegin();
         toplevelIt != event->toplevels.cend();
         ++toplevelIt) {
        const uint32_t identifier = *toplevelIt;
        for (auto surfaceIt = d->m_surfaces.cbegin(); surfaceIt != d->m_surfaces.cend();
             ++surfaceIt) {
            bool found = false;
            for (auto *handle : surfaceIt->second->handles) {
                if (handle->identifier() == identifier) {
                    surfaces.push_back(surfaceIt->first);
                    found = true;
                    break;
                }
            }
            if (found) {
                break;
            }
        }
    };

    Q_EMIT requestDockPreview(surfaces,
                              dockPreview->relativeSurface(),
                              QPoint(event->x, event->y),
                              static_cast<PreviewDirection>(event->direction));
}

void ForeignToplevelManagerInterfaceV1::handleDockPreviewShowTooltip(treeland_dock_preview_tooltip_event *event)
{
    DockPreviewContextV1 *dockPreview = static_cast<DockPreviewContextV1 *>(sender());
    Q_EMIT requestDockPreviewTooltip(
        event->tooltip,
        dockPreview->relativeSurface(),
        QPoint(event->x, event->y),
        static_cast<PreviewDirection>(event->direction));
}

class DockPreviewContextV1Private;

struct dock_preview_surface_destroy_wrapper {
    wl_listener listener;
    DockPreviewContextV1Private *d;
};

class DockPreviewContextV1Private
    : public QtWaylandServer::treeland_dock_preview_context_v1
{
public:
    DockPreviewContextV1Private(DockPreviewContextV1 *_q, wl_resource *resource, wlr_surface *_relativeSurface);
    ~DockPreviewContextV1Private() override;

    DockPreviewContextV1 *q = nullptr;
    wlr_surface *relativeSurface = nullptr;
    dock_preview_surface_destroy_wrapper surfaceDestroyWrapper = {};

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void show(Resource *resource, wl_array *surfaces, int32_t x, int32_t y, uint32_t direction) override;
    void show_tooltip(Resource *resource, const QString &tooltip, int32_t x, int32_t y, uint32_t direction) override;
    void close(Resource *resource) override;
};

DockPreviewContextV1Private::DockPreviewContextV1Private(DockPreviewContextV1 *_q, wl_resource *resource, wlr_surface *_relativeSurface)
    : QtWaylandServer::treeland_dock_preview_context_v1(resource)
    , q(_q)
    , relativeSurface(_relativeSurface)
{
    if (relativeSurface) {
        surfaceDestroyWrapper.d = this;
        surfaceDestroyWrapper.listener.notify = [](wl_listener *listener, void *) {
            dock_preview_surface_destroy_wrapper *wrapper;
            wrapper = wl_container_of(listener, wrapper, listener);
            wl_list_remove(&wrapper->listener.link);
            wl_list_init(&wrapper->listener.link);
            wrapper->d->relativeSurface = nullptr;
            wl_resource_destroy(wrapper->d->resource()->handle);
        };
        wl_signal_add(&relativeSurface->events.destroy, &surfaceDestroyWrapper.listener);
    }
}

DockPreviewContextV1Private::~DockPreviewContextV1Private()
{
    wl_list_remove(&surfaceDestroyWrapper.listener.link);
}

void DockPreviewContextV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->beforeDestroy();
    delete q;
}

void DockPreviewContextV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void DockPreviewContextV1Private::show([[maybe_unused]] Resource *resource, wl_array *surfaces, int32_t x, int32_t y, uint32_t direction)
{
    if (!relativeSurface) {
        return;
    }

    std::vector<uint32_t> s;
    const uint32_t *data = reinterpret_cast<const uint32_t *>(surfaces->data);
    const size_t count = surfaces->size / sizeof(uint32_t);
    for (size_t i = 0; i != count; ++i) {
        s.push_back(data[i]);
    }

    if (!surfaces->size)
        qCCritical(treelandProtocol) << "Got empty surface list for dock preview!";

    treeland_dock_preview_context_v1_preview_event event = {
        .toplevels = s,
        .x = x,
        .y = y,
        .direction = static_cast<treeland_dock_preview_context_v1_direction>(direction),
    };

    Q_EMIT q->requestShow(&event);
}

void DockPreviewContextV1Private::show_tooltip([[maybe_unused]] Resource *resource, const QString &tooltip, int32_t x, int32_t y, uint32_t direction)
{
    if (!relativeSurface) {
        return;
    }

    treeland_dock_preview_tooltip_event event = {
        .tooltip = tooltip,
        .x = x,
        .y = y,
        .direction = static_cast<treeland_dock_preview_context_v1_direction>(direction),
    };

    Q_EMIT q->requestShowTooltip(&event);
}

void DockPreviewContextV1Private::close([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestClose();
}

DockPreviewContextV1::~DockPreviewContextV1() = default;

wl_resource *DockPreviewContextV1::resource() const
{
    return d->resource()->handle;
}

WSurface *DockPreviewContextV1::relativeSurface() const
{
    return WSurface::fromHandle(d->relativeSurface);
}

void DockPreviewContextV1::enter()
{
    d->send_enter();
}

void DockPreviewContextV1::leave()
{
    d->send_leave();
}

DockPreviewContextV1 *DockPreviewContextV1::get(wl_resource *resource)
{
    for (auto *context : s_dockPreviewContexts) {
        if (context->resource() == resource) {
            return context;
        }
    }

    return nullptr;
}

DockPreviewContextV1 *DockPreviewContextV1::getDockPreviewContext(WSurface *relativeSurface)
{
    for (auto *context : s_dockPreviewContexts) {
        if (context->relativeSurface() == relativeSurface) {
            return context;
        }
    }

    return nullptr;
}

DockPreviewContextV1::DockPreviewContextV1(wl_resource *resource, wlr_surface *_relativeSurface)
    : QObject(nullptr)
    , d(new DockPreviewContextV1Private(this, resource, _relativeSurface))
{
}

class ForeignToplevelHandleV1Private
    : public QtWaylandServer::treeland_foreign_toplevel_handle_v1
{
public:
    ForeignToplevelHandleV1Private(ForeignToplevelHandleV1 *_q,
                                   ForeignToplevelManagerInterfaceV1 *_manager,
                                   wl_resource *resource,
                                   SurfaceEntry *_entry);
    ~ForeignToplevelHandleV1Private() override;

    ForeignToplevelHandleV1 *q = nullptr;
    ForeignToplevelManagerInterfaceV1 *manager = nullptr;
    SurfaceEntry *entry = nullptr;

    wl_event_source *idle_source{ nullptr };

    QString title;
    QString app_id;
    uint32_t identifier;
    pid_t pid;

    ForeignToplevelHandleV1 *parent{ nullptr };
    QList<treeland_foreign_toplevel_handle_v1_output> outputs;
    ForeignToplevelHandleV1::States state;
protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_maximized(Resource *resource) override;
    void unset_maximized(Resource *resource) override;
    void set_minimized(Resource *resource) override;
    void unset_minimized(Resource *resource) override;
    void activate(Resource *resource, struct ::wl_resource *seat) override;
    void close(Resource *resource) override;
    void set_rectangle(Resource *resource, struct ::wl_resource *surface, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void set_fullscreen(Resource *resource, struct ::wl_resource *output) override;
    void unset_fullscreen(Resource *resource) override;
};

ForeignToplevelHandleV1Private::ForeignToplevelHandleV1Private(ForeignToplevelHandleV1 *_q,
                                                               ForeignToplevelManagerInterfaceV1 *_manager,
                                                               wl_resource *resource,
                                                               SurfaceEntry *_entry)
    : QtWaylandServer::treeland_foreign_toplevel_handle_v1(resource)
    , q(_q)
    , manager(_manager)
    , entry(_entry)
{
}

ForeignToplevelHandleV1Private::~ForeignToplevelHandleV1Private()
{
    if (idle_source) {
        wl_event_source_remove(idle_source);
        idle_source = nullptr;
    }
    send_done();
}

void ForeignToplevelHandleV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    if (manager) {
        manager->releaseHandle(q);
    }

    delete q;
}

void ForeignToplevelHandleV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void ForeignToplevelHandleV1Private::set_maximized([[maybe_unused]] Resource *resource)
{
    struct treeland_foreign_toplevel_handle_v1_maximized_event event = {
        .maximized = true,
    };

    Q_EMIT q->requestMaximize(&event);
}

void ForeignToplevelHandleV1Private::unset_maximized([[maybe_unused]] Resource *resource)
{
    struct treeland_foreign_toplevel_handle_v1_maximized_event event = {
        .maximized = false,
    };

    Q_EMIT q->requestMaximize(&event);
}

void ForeignToplevelHandleV1Private::set_minimized([[maybe_unused]] Resource *resource)
{
    struct treeland_foreign_toplevel_handle_v1_minimized_event event = {
        .minimized = true,
    };

    Q_EMIT q->requestMinimize(&event);
}

void ForeignToplevelHandleV1Private::unset_minimized([[maybe_unused]] Resource *resource)
{
    struct treeland_foreign_toplevel_handle_v1_minimized_event event = {
        .minimized = false,
    };

    Q_EMIT q->requestMinimize(&event);
}

void ForeignToplevelHandleV1Private::activate(Resource *resource, struct ::wl_resource *seat)
{
    const wlr_seat_client *seat_client = wlr_seat_client_from_resource(seat);
    if (!seat_client) {
         wl_resource_post_error(resource->handle, 0, "wlr_seat_client_from_resource failed!");
        return;
    }

    struct treeland_foreign_toplevel_handle_v1_activated_event event = {
        .seat = seat_client->seat,
    };

    Q_EMIT q->requestActivate(&event);
}

void ForeignToplevelHandleV1Private::close([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->requestClose();
}

void ForeignToplevelHandleV1Private::set_rectangle(Resource *resource, struct ::wl_resource *surface, int32_t x, int32_t y, int32_t width, int32_t height)
{
    if (width < 0 || height < 0) {
        wl_resource_post_error(resource->handle,
                               TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_ERROR_INVALID_RECTANGLE,
                               "invalid rectangle passed to set_rectangle: width/height < 0");
        return;
    }

    struct treeland_foreign_toplevel_handle_v1_set_rectangle_event event = {
        .surface = wlr_surface_from_resource(surface),
        .x = x,
        .y = y,
        .width = width,
        .height = height,
    };

    Q_EMIT q->rectangleChanged(&event);
}

void ForeignToplevelHandleV1Private::set_fullscreen(Resource *resource, struct ::wl_resource *output)
{
    struct wlr_output *wlrOutput = nullptr;
    if (output) {
        wlrOutput = wlr_output_from_resource(output);

        if (!wlrOutput) {
            wl_resource_post_error(resource->handle, 0, "wlr_output_from_resource failed!");
            return;
        }
    }

    struct treeland_foreign_toplevel_handle_v1_fullscreen_event event = {
        .fullscreen = true,
        .output = wlrOutput,
    };

    Q_EMIT q->requestFullscreen(&event);
}

void ForeignToplevelHandleV1Private::unset_fullscreen([[maybe_unused]] Resource *resource)
{
    struct treeland_foreign_toplevel_handle_v1_fullscreen_event event = {
        .fullscreen = false,
        .output = nullptr,
    };

    Q_EMIT q->requestFullscreen(&event);
}

ForeignToplevelHandleV1::~ForeignToplevelHandleV1() = default;

wl_resource *ForeignToplevelHandleV1::resource() const
{
    return d->resource()->handle;
}

void ForeignToplevelHandleV1::set_title(const QString &title)
{
    if (d->title == title) {
        return;
    }

    d->title = title;
    d->send_title(title);
    update_idle_source();
}

void ForeignToplevelHandleV1::set_app_id(const QString &app_id)
{
    if (d->app_id == app_id) {
        return;
    }

    d->app_id = app_id;
    d->send_app_id(app_id);
    update_idle_source();
}

void ForeignToplevelHandleV1::set_pid(const pid_t pid)
{
    d->pid = pid;
    d->send_pid(pid);
    update_idle_source();
}

void ForeignToplevelHandleV1::set_identifier(uint32_t identifier)
{
    d->identifier = identifier;
    d->send_identifier(identifier);
    update_idle_source();
}

uint32_t ForeignToplevelHandleV1::identifier() const
{
    return d->identifier;
}

void ForeignToplevelHandleV1::output_enter(QW_NAMESPACE::qw_output *output)
{
    if (std::any_of(d->outputs.begin(),
                    d->outputs.end(),
                    [output](const treeland_foreign_toplevel_handle_v1_output &toplevel_output) {
                        return toplevel_output.output == output;
                    }))
        return;

    auto toplevel_output =
        treeland_foreign_toplevel_handle_v1_output{ .output = output, .toplevel = this };
    d->outputs.append(toplevel_output);

    connect(output, &qw_output::notify_bind, this, [toplevel_output](wlr_output_event_bind *event) {
        const wl_client *client = wl_resource_get_client(event->resource);
        if (wl_resource_get_client(toplevel_output.toplevel->resource()) == client) {
            toplevel_output.toplevel->send_output(toplevel_output.output, true);
        }

        toplevel_output.toplevel->update_idle_source();
    });

    connect(output, &qw_output::before_destroy, this, [toplevel_output]() {
        toplevel_output.toplevel->output_leave(toplevel_output.output);
    });
    send_output(output, true);
}

void ForeignToplevelHandleV1::output_leave(QW_NAMESPACE::qw_output *output)
{
    d->outputs.removeIf([output](const treeland_foreign_toplevel_handle_v1_output &handle_output) {
        return handle_output.output == output;
    });

    send_output(output, false);
}

void ForeignToplevelHandleV1::set_maximized(bool maximized)
{
    if (d->state.testFlag(State::Maximized) == maximized) {
        return;
    }
    d->state.setFlag(State::Maximized, maximized);
    send_state();
}

void ForeignToplevelHandleV1::set_minimized(bool minimized)
{
    if (d->state.testFlag(State::Minimized) == minimized) {
        return;
    }
    d->state.setFlag(State::Minimized, minimized);
    send_state();
}

void ForeignToplevelHandleV1::set_activated(bool activated)
{
    if (d->state.testFlag(State::Activated) == activated) {
        return;
    }
    d->state.setFlag(State::Activated, activated);
    send_state();
}

void ForeignToplevelHandleV1::set_fullscreen(bool fullscreen)
{
    if (d->state.testFlag(State::Fullscreen) == fullscreen) {
        return;
    }
    d->state.setFlag(State::Fullscreen, fullscreen);
    send_state();
}

void ForeignToplevelHandleV1::set_attention(bool attention)
{
    if (d->state.testFlag(State::Attention) == attention) {
        return;
    }
    d->state.setFlag(State::Attention, attention);
    send_state();
}

void ForeignToplevelHandleV1::set_parent(ForeignToplevelHandleV1 *parent)
{
    if (d->parent == parent) {
        return;
    }

    d->send_parent(parent ? parent->resource() : nullptr);
    d->parent = parent;
    update_idle_source();
}

void ForeignToplevelHandleV1::reset_idle_source()
{
    d->idle_source = nullptr;
}

void ForeignToplevelHandleV1::send_done()
{
    d->send_done();
}

void ForeignToplevelHandleV1::send_closed()
{
    d->send_closed();
}

void ForeignToplevelHandleV1::send_state()
{
    QByteArray ba;
    auto push = [&](uint32_t v) {
        ba.append(reinterpret_cast<const char*>(&v), sizeof(v));
    };

    if (d->state.testFlag(State::Maximized))
        push(TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED);

    if (d->state.testFlag(State::Minimized))
        push(TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED);

    if (d->state.testFlag(State::Activated))
        push(TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED);

    if (d->state.testFlag(State::Fullscreen))
        push(TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN);

    if (d->state.testFlag(State::Attention))
        push(TREELAND_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ATTENTION);

    d->send_state(ba);

    update_idle_source();
}

void ForeignToplevelHandleV1::send_output(qw_output *output, bool enter) {
    const wl_client *client = wl_resource_get_client(resource());
    struct wl_resource *output_resource;

    wl_resource_for_each(output_resource, &output->handle()->resources)
    {
        if (wl_resource_get_client(output_resource) == client) {
            if (enter) {
                treeland_foreign_toplevel_handle_v1_send_output_enter(resource(), output_resource);
            } else {
                treeland_foreign_toplevel_handle_v1_send_output_leave(resource(), output_resource);
            }
        }
    }

    update_idle_source();
}

ForeignToplevelHandleV1 *ForeignToplevelHandleV1::get(wl_resource *resource)
{
    auto *res = QtWaylandServer::treeland_foreign_toplevel_handle_v1::Resource::fromResource(resource);
    if (!res || !res->object()) {
        return nullptr;
    }
    auto *priv = static_cast<ForeignToplevelHandleV1Private *>(res->object());
    return priv->q;
}

SurfaceEntry *ForeignToplevelHandleV1::entry() const
{
    return d->entry;
}

void ForeignToplevelHandleV1::clearEntry()
{
    d->entry = nullptr;
}

ForeignToplevelHandleV1::ForeignToplevelHandleV1(ForeignToplevelManagerInterfaceV1 *manager,
                                                  wl_resource *resource,
                                                  SurfaceEntry *entry)
    : QObject(nullptr)
    , d(new ForeignToplevelHandleV1Private(this, manager, resource, entry))
{
}

static void toplevel_idle_send_done(void *data)
{
    auto *toplevel = static_cast<ForeignToplevelHandleV1 *>(data);
    toplevel->send_done();
    toplevel->reset_idle_source();
}

void ForeignToplevelHandleV1::update_idle_source()
{
    if (d->idle_source || !d->manager) {
        return;
    }

    d->idle_source = wl_event_loop_add_idle(d->manager->eventLoop(), toplevel_idle_send_done, this);
}
