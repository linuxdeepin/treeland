// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "windowtransitionmanagerinterfacev1.h"

#include "common/treelandlogging.h"
#include "modules/activation/wayland-xdg-activation-v1-server-protocol.h"
#include "qwayland-server-treeland-window-transition-unstable-v1.h"
#include "surface/surfacewrapper.h"

#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>
#include <wserver.h>
#include <wsurface.h>

#include <QDeadlineTimer>
#include <QHash>
#include <QPointer>
#include <QRectF>
#include <QTimer>

WAYLIB_SERVER_USE_NAMESPACE
using namespace Qt::StringLiterals;

namespace {
// A consumed transition rect stays pending for the same lifetime as an
// activation token; sweep it once that lifetime elapses so a token that is
// committed but never activated does not leak the rect object.
constexpr int kPendingRectLifetimeMs = 60'000;
} // namespace

class WindowTransitionManagerInterfaceV1Private;

// wlroots role assigned to the client-provided transition source surface.
// The role has no wire object: it is assigned implicitly by
// treeland_window_transition_rect_v1.set_source_surface and only marks the
// surface so that it stays out of the normal scene.
static const wlr_surface_role transitionSourceRole = {
    .name = "treeland_window_transition_source_v1",
    .no_object = true,
    .client_commit = nullptr,
    .commit = nullptr,
    .map = nullptr,
    .unmap = nullptr,
    .destroy = nullptr,
};

// Owns the WSurface wrapper of a transition source surface for the whole
// lifetime of the native wl_surface. The transition source role is exclusive,
// so a surface used as a source is never wrapped by anything else; every rect
// that references it shares this single wrapper and must not delete it while
// another rect may still use it.
struct SourceSurfaceEntry
{
    WindowTransitionManagerInterfaceV1Private *manager = nullptr;
    wlr_surface *handle = nullptr;
    WSurface *wrapper = nullptr;
    wl_listener destroyListener {};
};

class WindowTransitionRectV1 : public QtWaylandServer::treeland_window_transition_rect_v1
{
    friend class WindowTransitionManagerInterfaceV1;

public:
    WindowTransitionRectV1(WindowTransitionManagerInterfaceV1Private *manager,
                           wl_resource *tokenResource,
                           wl_resource *resource)
        : QtWaylandServer::treeland_window_transition_rect_v1(resource)
        , m_manager(manager)
        , m_tokenResource(tokenResource)
    {
        if (m_tokenResource) {
            m_tokenDestroyListener.notify = tokenResourceDestroyed;
            wl_resource_add_destroy_listener(m_tokenResource, &m_tokenDestroyListener);
        }
    }

    ~WindowTransitionRectV1();

    void notifyDiscarded()
    {
        send_closed();
    }

    bool isExpired() const
    {
        return m_expiry.hasExpired();
    }

    void associate(SurfaceWrapper *targetWrapper, SurfaceWrapper *originWrapper)
    {
        m_targetWrapper = targetWrapper;
        if (m_targetWrapper) {
            m_targetWrapper->setWindowTransitionRect(m_rect, originWrapper);
            m_targetWrapper->setWindowTransitionSourceSurface(m_sourceSurface.data());

            m_targetInvalidatedConnection =
                QObject::connect(m_targetWrapper.data(),
                                 &SurfaceWrapper::aboutToBeInvalidated,
                                 [this] {
                                     send_closed();
                                 });
        }
    }

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource(Resource *) override
    {
        delete this;
    }

    void set_geometry(Resource * /*resource*/,
                      int32_t x,
                      int32_t y,
                      int32_t width,
                      int32_t height) override;
    void set_source_surface(Resource * /*resource*/, struct ::wl_resource *surface) override;

private:
    static void tokenResourceDestroyed(wl_listener *listener, void *data);

    void clearSourceSurface();
    void releaseSourceSurface();

    WindowTransitionManagerInterfaceV1Private *m_manager;
    wl_resource *m_tokenResource;
    QString m_token;
    QDeadlineTimer m_expiry;
    wl_listener m_tokenDestroyListener;
    QRectF m_rect;
    uint m_geometrySet : 1 = 0;
    uint m_consumed : 1 = 0;
    QMetaObject::Connection m_targetInvalidatedConnection;
    QPointer<SurfaceWrapper> m_targetWrapper;

    // Owned by the manager, not by this rect; only a weak reference.
    QPointer<WSurface> m_sourceSurface;
};

class WindowTransitionManagerInterfaceV1Private
    : public QtWaylandServer::treeland_window_transition_manager_v1
{
public:
    explicit WindowTransitionManagerInterfaceV1Private()
        : QtWaylandServer::treeland_window_transition_manager_v1()
    {
    }

    ~WindowTransitionManagerInterfaceV1Private();

    wl_global *globalHandle() const
    {
        return m_global;
    }

    // Returns the shared WSurface wrapper for a transition source surface,
    // creating and taking ownership of it on first use.
    WSurface *acquireSourceSurface(wlr_surface *surface);

    QHash<wl_resource *, WindowTransitionRectV1 *> m_committedRects;
    QHash<QString, WindowTransitionRectV1 *> m_pendingRects;
    QHash<wlr_surface *, SourceSurfaceEntry *> m_sourceSurfaces;

protected:
    void destroy_global() override
    {
        qCDebug(lcTlWindowTransition) << "treeland_window_transition_manager_v1 global destroyed";
    }

    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void get_window_transition_rect(Resource *resource,
                                    uint32_t rect_id,
                                    struct ::wl_resource *token) override
    {
        if (!token || wl_resource_get_interface(token) != &xdg_activation_token_v1_interface) {
            wl_resource_post_error(resource->handle,
                                   WL_DISPLAY_ERROR_INVALID_OBJECT,
                                   "get_window_transition_rect: token is not an"
                                   " xdg_activation_token_v1");
            return;
        }
        auto *rectResource = wl_resource_create(resource->client(),
                                                &treeland_window_transition_rect_v1_interface,
                                                resource->version(),
                                                rect_id);
        if (!rectResource) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }
        new WindowTransitionRectV1(this, token, rectResource);
    }
};

namespace {
void sourceSurfaceDestroyed(wl_listener *listener, void * /*data*/)
{
    SourceSurfaceEntry *entry;
    entry = wl_container_of(listener, entry, destroyListener);
    wl_list_remove(&entry->destroyListener.link);
    entry->manager->m_sourceSurfaces.remove(entry->handle);
    // The WSurface wrapper must be destroyed from the native destroy callback,
    // while the handle is still valid.
    delete entry->wrapper;
    delete entry;
}
} // namespace

WindowTransitionManagerInterfaceV1Private::~WindowTransitionManagerInterfaceV1Private()
{
    const auto entries = m_sourceSurfaces;
    m_sourceSurfaces.clear();
    for (auto *entry : entries) {
        wl_list_remove(&entry->destroyListener.link);
        delete entry->wrapper;
        delete entry;
    }
}

WSurface *WindowTransitionManagerInterfaceV1Private::acquireSourceSurface(wlr_surface *surface)
{
    if (auto *entry = m_sourceSurfaces.value(surface))
        return entry->wrapper;

    auto *entry = new SourceSurfaceEntry;
    entry->manager = this;
    entry->handle = surface;
    entry->wrapper = new WSurface(surface);
    entry->destroyListener.notify = sourceSurfaceDestroyed;
    wl_signal_add(&surface->events.destroy, &entry->destroyListener);
    m_sourceSurfaces.insert(surface, entry);
    return entry->wrapper;
}

WindowTransitionRectV1::~WindowTransitionRectV1()
{
    if (m_targetInvalidatedConnection)
        QObject::disconnect(m_targetInvalidatedConnection);

    if (m_targetWrapper) {
        m_targetWrapper->clearWindowTransitionSource();
    }

    releaseSourceSurface();

    if (m_manager && !m_token.isEmpty())
        m_manager->m_pendingRects.remove(m_token);

    if (m_tokenResource) {
        wl_list_remove(&m_tokenDestroyListener.link);
        if (m_manager && m_geometrySet && !m_consumed)
            m_manager->m_committedRects.remove(m_tokenResource);
    }
}

void WindowTransitionRectV1::tokenResourceDestroyed(wl_listener *listener, void * /*data*/)
{
    WindowTransitionRectV1 *self;
    self = wl_container_of(listener, self, m_tokenDestroyListener);
    if (self->m_manager && self->m_geometrySet && !self->m_consumed)
        self->m_manager->m_committedRects.remove(self->m_tokenResource);
    self->m_tokenResource = nullptr;
    wl_list_remove(&self->m_tokenDestroyListener.link);
    wl_list_init(&self->m_tokenDestroyListener.link);
}

void WindowTransitionRectV1::set_geometry(Resource *resource,
                                          int32_t x,
                                          int32_t y,
                                          int32_t width,
                                          int32_t height)
{
    if (width < 0 || height < 0) {
        wl_resource_post_error(resource->handle,
                               error_invalid_geometry,
                               "set_geometry with negative size %dx%d",
                               width,
                               height);
        return;
    }
    m_rect = QRectF(x, y, width, height);
    m_geometrySet = true;

    if (m_targetWrapper)
        m_targetWrapper->updateWindowTransitionRect(m_rect);

    if (!m_consumed && m_tokenResource && m_manager)
        m_manager->m_committedRects.insert(m_tokenResource, this);

    // Let a client that renders on frame callbacks produce a frame for the
    // newly configured rectangle. No-op when the client has no pending
    // callback on the source surface.
    if (m_sourceSurface)
        m_sourceSurface->notifyFrameDone();
}

void WindowTransitionRectV1::set_source_surface(Resource *resource,
                                                struct ::wl_resource *surfaceResource)
{
    if (!surfaceResource) {
        clearSourceSurface();
        return;
    }

    wlr_surface *surface = wlr_surface_from_resource(surfaceResource);
    if (!surface
        || wl_resource_get_client(surfaceResource) != wl_resource_get_client(resource->handle)) {
        wl_resource_post_error(resource->handle,
                               error_invalid_surface,
                               "set_source_surface: surface is not a wl_surface of this client");
        return;
    }

    if (m_sourceSurface && m_sourceSurface->handle() == surface)
        return;

    // The surface must not already carry a role. wlr_surface_set_role posts
    // the invalid_surface error itself when it cannot assign the role.
    if (!wlr_surface_set_role(surface,
                              &transitionSourceRole,
                              resource->handle,
                              error_invalid_surface)) {
        return;
    }

    releaseSourceSurface();

    // The manager owns the wrapper for the lifetime of the native surface, so
    // it can be shared by several rects and is never deleted while one of them
    // still references it.
    auto *wSurface = m_manager->acquireSourceSurface(surface);

    m_sourceSurface = wSurface;

    if (m_targetWrapper)
        m_targetWrapper->setWindowTransitionSourceSurface(wSurface);

    wSurface->notifyFrameDone();
}

void WindowTransitionRectV1::clearSourceSurface()
{
    releaseSourceSurface();
    if (m_targetWrapper)
        m_targetWrapper->setWindowTransitionSourceSurface(nullptr);
}

void WindowTransitionRectV1::releaseSourceSurface()
{
    // The wrapper is owned by the manager; only drop our weak reference.
    m_sourceSurface = nullptr;
}

WindowTransitionManagerInterfaceV1::WindowTransitionManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new WindowTransitionManagerInterfaceV1Private())
    , m_pendingSweepTimer(this)
{
    m_pendingSweepTimer.setInterval(kPendingRectLifetimeMs / 2);
    connect(&m_pendingSweepTimer,
            &QTimer::timeout,
            this,
            &WindowTransitionManagerInterfaceV1::sweepPendingRects);
}

WindowTransitionManagerInterfaceV1::~WindowTransitionManagerInterfaceV1() = default;

QByteArrayView WindowTransitionManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

void WindowTransitionManagerInterfaceV1::ensurePendingSweepRunning()
{
    if (!d->m_pendingRects.isEmpty() && !m_pendingSweepTimer.isActive())
        m_pendingSweepTimer.start();
}

void WindowTransitionManagerInterfaceV1::sweepPendingRects()
{
    for (auto it = d->m_pendingRects.begin(); it != d->m_pendingRects.end();) {
        if (it.value()->isExpired()) {
            it.value()->notifyDiscarded();
            it = d->m_pendingRects.erase(it);
        } else {
            ++it;
        }
    }
    if (d->m_pendingRects.isEmpty())
        m_pendingSweepTimer.stop();
}

void WindowTransitionManagerInterfaceV1::takeCommittedRect(const QString &token,
                                                           wl_resource *tokenResource)
{
    if (!tokenResource)
        return;
    auto it = d->m_committedRects.find(tokenResource);
    if (it == d->m_committedRects.end())
        return;
    auto *rectObj = it.value();
    d->m_committedRects.erase(it);
    rectObj->m_consumed = true;
    rectObj->m_token = token;
    rectObj->m_expiry = QDeadlineTimer(kPendingRectLifetimeMs);
    d->m_pendingRects.insert(token, rectObj);
    ensurePendingSweepRunning();
}

bool WindowTransitionManagerInterfaceV1::hasPendingWindowTransitionRect(const QString &token) const
{
    return d->m_pendingRects.contains(token);
}

bool WindowTransitionManagerInterfaceV1::associatePendingRect(const QString &token,
                                                              SurfaceWrapper *targetWrapper,
                                                              SurfaceWrapper *originWrapper)
{
    auto it = d->m_pendingRects.find(token);
    if (it == d->m_pendingRects.end())
        return false;
    auto *rectObj = it.value();
    d->m_pendingRects.erase(it);
    rectObj->associate(targetWrapper, originWrapper);
    return true;
}

void WindowTransitionManagerInterfaceV1::discardPendingRect(const QString &token)
{
    auto it = d->m_pendingRects.find(token);
    if (it == d->m_pendingRects.end())
        return;
    auto *rectObj = it.value();
    d->m_pendingRects.erase(it);
    rectObj->notifyDiscarded();
}

void WindowTransitionManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void WindowTransitionManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *WindowTransitionManagerInterfaceV1::global() const
{
    return d->globalHandle();
}
