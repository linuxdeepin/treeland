// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "launchanimationmanagerinterfacev1.h"
#include "common/treelandlogging.h"
#include "qwayland-server-treeland-launch-animation-v1.h"

#include <wserver.h>

#include <wayland-server-core.h>

#include <QHash>

WAYLIB_SERVER_USE_NAMESPACE
using namespace Qt::StringLiterals;

// ---------------------------------------------------------------------------
// Forward declaration
// ---------------------------------------------------------------------------

class LaunchAnimationManagerInterfaceV1Private;

// A committed launch rect must outlive its rect object: the protocol makes the
// rect inert (and thus freely destroyable) after commit(), while the associated
// xdg_activation_token_v1 consumes the rectangle later via
// LaunchAnimationManagerInterfaceV1::takeCommittedRect(). This guard owns the
// cache entry's cleanup independently of the rect object: it is registered on
// the token resource at commit() time and removes the entry when the token is
// destroyed without being consumed.
struct LaunchRectCommittedGuard
{
    wl_listener listener;
    LaunchAnimationManagerInterfaceV1Private *manager;
    wl_resource *tokenResource;
};

// ---------------------------------------------------------------------------
// Launch rectangle object — one per treeland_launch_rect_v1 resource
// ---------------------------------------------------------------------------

class LaunchRectV1 : public QtWaylandServer::treeland_launch_rect_v1
{
public:
    LaunchRectV1(LaunchAnimationManagerInterfaceV1Private *manager,
                 wl_resource *tokenResource,
                 wl_resource *resource)
        : QtWaylandServer::treeland_launch_rect_v1(resource)
        , m_manager(manager)
        , m_tokenResource(tokenResource)
    {
        // Clean up the committed-rect cache entry when the associated
        // xdg_activation_token_v1 resource is destroyed, so a token that is
        // destroyed without ever being committed does not leave a dangling
        // wl_resource* key behind.  A null token (client passed id=0) must
        // never reach here — get_launch_rect rejects it earlier — but guard
        // defensively to avoid a NULL dereference in wl_signal_add.
        if (m_tokenResource) {
            m_tokenDestroyListener.notify = tokenResourceDestroyed;
            wl_resource_add_destroy_listener(m_tokenResource, &m_tokenDestroyListener);
        }
    }

    ~LaunchRectV1();

protected:
    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void destroy_resource(Resource *) override
    {
        delete this;
    }

    void set_geometry(Resource * /*resource*/, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void commit(Resource *resource) override;

public:
    // Used by LaunchAnimationManagerInterfaceV1::takeCommittedRect() to look up
    // and detach the commit-time guard registered on the token resource.
    static void committedTokenDestroyed(wl_listener *listener, void *data);

private:
    static void tokenResourceDestroyed(wl_listener *listener, void *data);

    LaunchAnimationManagerInterfaceV1Private *m_manager;
    wl_resource *m_tokenResource;
    wl_listener m_tokenDestroyListener;
    QRectF m_rect;
    bool m_committed = false;
};

// ---------------------------------------------------------------------------
// Manager private — implements the global interface
// ---------------------------------------------------------------------------

class LaunchAnimationManagerInterfaceV1Private
    : public QtWaylandServer::treeland_launch_animation_manager_v1
{
public:
    explicit LaunchAnimationManagerInterfaceV1Private()
        : QtWaylandServer::treeland_launch_animation_manager_v1()
    {
    }

    wl_global *globalHandle() const { return m_global; }

    QHash<wl_resource *, QRectF> m_committedRects;

protected:
    void destroy_global() override
    {
        qCDebug(lcTlLaunchAnimation) << "treeland_launch_animation_manager_v1 global destroyed";
    }

    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void get_launch_rect(Resource *resource,
                         uint32_t launch_rect,
                         struct ::wl_resource *token) override
    {
        // Wayland allows a client to pass id=0 for an object argument, which
        // arrives as NULL here.  A null token would cause a NULL dereference
        // in the LaunchRectV1 constructor (wl_resource_add_destroy_listener →
        // wl_signal_add dereferences resource->destroy_signal), crashing the
        // compositor.  Reject it before any resource is created.
        if (!token) {
            wl_resource_post_error(resource->handle,
                                   WL_DISPLAY_ERROR_INVALID_OBJECT,
                                   "get_launch_rect: token is null");
            return;
        }
        auto *rectResource = wl_resource_create(resource->client(),
                                                &treeland_launch_rect_v1_interface,
                                                resource->version(),
                                                launch_rect);
        if (!rectResource) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }
        // LaunchRectV1 self-destructs in destroy_resource()
        new LaunchRectV1(this, token, rectResource);
    }

};

// ---------------------------------------------------------------------------
// LaunchRectV1 method definitions (after Private is fully defined)
// ---------------------------------------------------------------------------

LaunchRectV1::~LaunchRectV1()
{
    // A committed rect must survive this object's destruction: the protocol
    // declares the rect inert (destroyable) after commit(), and the token
    // consumes the rectangle later via takeCommittedRect(). Only detach this
    // object's own token-destroy listener; the committed cache entry (if any)
    // is owned by a LaunchRectCommittedGuard registered in commit().  When the
    // token was null the listener was never registered, so its link is
    // uninitialized and must not be removed.
    if (m_tokenResource)
        wl_list_remove(&m_tokenDestroyListener.link);
}

void LaunchRectV1::tokenResourceDestroyed(wl_listener *listener, void * /*data*/)
{
    LaunchRectV1 *self;
    self = wl_container_of(listener, self, m_tokenDestroyListener);
    // The token resource is gone. Null our handle so a later commit() does not
    // cache under a recycled wl_resource address. The committed entry itself
    // (if any) is cleaned by the LaunchRectCommittedGuard registered in
    // commit(), which also listens on the token resource.
    self->m_tokenResource = nullptr;
    wl_list_remove(&self->m_tokenDestroyListener.link);
    wl_list_init(&self->m_tokenDestroyListener.link);
}

void LaunchRectV1::committedTokenDestroyed(wl_listener *listener, void * /*data*/)
{
    LaunchRectCommittedGuard *guard;
    guard = wl_container_of(listener, guard, listener);
    // The token was destroyed without its rectangle being consumed; drop the
    // cached entry so a recycled wl_resource address is never mistaken for a
    // stale one, then free the guard.
    if (guard->manager)
        guard->manager->m_committedRects.remove(guard->tokenResource);
    wl_list_remove(&guard->listener.link);
    delete guard;
}

void LaunchRectV1::set_geometry(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    if (m_committed) {
        wl_resource_post_error(resource->handle,
                               error_already_committed,
                               "set_geometry requested after commit");
        return;
    }
    if (width <= 0 || height <= 0) {
        wl_resource_post_error(resource->handle,
                               error_invalid_geometry,
                               "set_geometry with non-positive size %dx%d", width, height);
        return;
    }
    m_rect = QRectF(x, y, width, height);
}

void LaunchRectV1::commit(Resource *resource)
{
    if (m_committed) {
        wl_resource_post_error(resource->handle,
                               error_already_committed,
                               "commit() called a second time on this launch rect");
        return;
    }
    if (!m_rect.isValid()) {
        wl_resource_post_error(resource->handle,
                               error_no_geometry,
                               "commit() called before a valid set_geometry");
        return;
    }
    m_committed = true;
    // m_tokenResource is null only if the associated token was already
    // destroyed; in that case the rect has no destination and is not cached.
    if (m_tokenResource) {
        m_manager->m_committedRects.insert(m_tokenResource, m_rect);
        // Register a guard on the token resource so the cached entry is removed
        // when the token is destroyed without being consumed, even if this rect
        // object has already been destroyed (the protocol allows destroying the
        // rect right after commit). takeCommittedRect() detaches and frees the
        // guard when the rectangle is consumed.
        auto *guard = new LaunchRectCommittedGuard;
        guard->manager = m_manager;
        guard->tokenResource = m_tokenResource;
        guard->listener.notify = committedTokenDestroyed;
        wl_resource_add_destroy_listener(m_tokenResource, &guard->listener);
    }
    qCDebug(lcTlLaunchAnimation) << "Launch rect committed:" << m_rect;
}

// ---------------------------------------------------------------------------
// Public class
// ---------------------------------------------------------------------------

LaunchAnimationManagerInterfaceV1::LaunchAnimationManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new LaunchAnimationManagerInterfaceV1Private())
{
}

LaunchAnimationManagerInterfaceV1::~LaunchAnimationManagerInterfaceV1() = default;

QByteArrayView LaunchAnimationManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

std::optional<QRectF> LaunchAnimationManagerInterfaceV1::takeCommittedRect(wl_resource *tokenResource)
{
    if (!tokenResource)
        return std::nullopt;
    auto it = d->m_committedRects.find(tokenResource);
    if (it == d->m_committedRects.end())
        return std::nullopt;
    auto rect = it.value();
    d->m_committedRects.erase(it);
    // Detach and free the guard registered in commit() so its token-destroy
    // callback does not fire on a now-recycled token resource.
    if (wl_listener *l = wl_resource_get_destroy_listener(
            tokenResource, &LaunchRectV1::committedTokenDestroyed)) {
        LaunchRectCommittedGuard *guard;
        guard = wl_container_of(l, guard, listener);
        wl_list_remove(&guard->listener.link);
        delete guard;
    }
    return rect;
}

void LaunchAnimationManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void LaunchAnimationManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *LaunchAnimationManagerInterfaceV1::global() const
{
    return d->globalHandle();
}
