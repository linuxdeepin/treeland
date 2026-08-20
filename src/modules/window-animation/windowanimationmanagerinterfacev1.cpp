// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "windowanimationmanagerinterfacev1.h"
#include "common/treelandlogging.h"
#include "qwayland-server-treeland-window-animation-v1.h"

#include <wserver.h>

#include <wayland-server-core.h>

#include <QHash>
#include <QPointer>

#include "surface/surfacewrapper.h"

WAYLIB_SERVER_USE_NAMESPACE
using namespace Qt::StringLiterals;

// ---------------------------------------------------------------------------
// Forward declaration
// ---------------------------------------------------------------------------

class WindowAnimationManagerInterfaceV1Private;

// ---------------------------------------------------------------------------
// Window animation rectangle object — one per treeland_window_animation_rect_v1 resource
// ---------------------------------------------------------------------------

class WindowAnimationRectV1 : public QtWaylandServer::treeland_window_animation_rect_v1
{
    friend class WindowAnimationManagerInterfaceV1;
public:
    WindowAnimationRectV1(WindowAnimationManagerInterfaceV1Private *manager,
                          wl_resource *tokenResource,
                          wl_resource *resource)
        : QtWaylandServer::treeland_window_animation_rect_v1(resource)
        , m_manager(manager)
        , m_tokenResource(tokenResource)
    {
        // Clean up the committed-rect cache entry when the associated
        // xdg_activation_token_v1 resource is destroyed, so a token that is
        // destroyed without being consumed does not leave a dangling entry.
        if (m_tokenResource) {
            m_tokenDestroyListener.notify = tokenResourceDestroyed;
            wl_resource_add_destroy_listener(m_tokenResource, &m_tokenDestroyListener);
        }
    }

    ~WindowAnimationRectV1();

    QRectF rect() const { return m_rect; }
    bool isCommitted() const { return m_committed; }

    /**
     * Associates this rect with the target wrapper (B's window) and the
     * originating wrapper (A's window). After association, subsequent
     * commit() calls push geometry updates to the target wrapper.
     */
    void associate(SurfaceWrapper *targetWrapper, SurfaceWrapper *originWrapper)
    {
        m_targetWrapper = targetWrapper;
        if (m_targetWrapper)
            m_targetWrapper->setWindowAnimationRect(m_rect, originWrapper);
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

    void set_geometry(Resource * /*resource*/, int32_t x, int32_t y, int32_t width, int32_t height) override;
    void commit(Resource *resource) override;

private:
    static void tokenResourceDestroyed(wl_listener *listener, void *data);

    WindowAnimationManagerInterfaceV1Private *m_manager;
    wl_resource *m_tokenResource;
    wl_listener m_tokenDestroyListener;
    QRectF m_rect;
    bool m_committed = false;
    bool m_consumed = false; // set when takeCommittedRect() removes this from the cache
    QPointer<SurfaceWrapper> m_targetWrapper;
};

// ---------------------------------------------------------------------------
// Manager private — implements the global interface
// ---------------------------------------------------------------------------

class WindowAnimationManagerInterfaceV1Private
    : public QtWaylandServer::treeland_window_animation_manager_v1
{
public:
    explicit WindowAnimationManagerInterfaceV1Private()
        : QtWaylandServer::treeland_window_animation_manager_v1()
    {
    }

    wl_global *globalHandle() const { return m_global; }

    QHash<wl_resource *, WindowAnimationRectV1 *> m_committedRects;
    WindowAnimationRectV1 *m_pendingRect = nullptr; // rect consumed by takeCommittedRect, awaiting association

protected:
    void destroy_global() override
    {
        qCDebug(lcTlWindowAnimation) << "treeland_window_animation_manager_v1 global destroyed";
    }

    void destroy(Resource *resource) override
    {
        wl_resource_destroy(resource->handle);
    }

    void get_window_animation_rect(Resource *resource,
                                   uint32_t rect_id,
                                   struct ::wl_resource *token) override
    {
        if (!token) {
            wl_resource_post_error(resource->handle,
                                   WL_DISPLAY_ERROR_INVALID_OBJECT,
                                   "get_window_animation_rect: token is null");
            return;
        }
        auto *rectResource = wl_resource_create(resource->client(),
                                                &treeland_window_animation_rect_v1_interface,
                                                resource->version(),
                                                rect_id);
        if (!rectResource) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }
        new WindowAnimationRectV1(this, token, rectResource);
    }
};

// ---------------------------------------------------------------------------
// WindowAnimationRectV1 method definitions (after Private is fully defined)
// ---------------------------------------------------------------------------

WindowAnimationRectV1::~WindowAnimationRectV1()
{
    // Notify the target wrapper that the rect is gone, so it falls back to
    // the default close animation.
    if (m_targetWrapper)
        m_targetWrapper->clearWindowAnimationRect();

    // Clear the manager's pending pointer if it points to us.
    if (m_manager && m_manager->m_pendingRect == this)
        m_manager->m_pendingRect = nullptr;

    // Remove from the committed-rect cache if still present (token not yet
    // consumed). If the token was already destroyed, m_tokenResource is null
    // and the cache entry was already removed by the token-destroy listener.
    if (m_tokenResource) {
        wl_list_remove(&m_tokenDestroyListener.link);
        if (m_manager && m_committed && !m_consumed)
            m_manager->m_committedRects.remove(m_tokenResource);
    }
}

void WindowAnimationRectV1::tokenResourceDestroyed(wl_listener *listener, void * /*data*/)
{
    WindowAnimationRectV1 *self;
    self = wl_container_of(listener, self, m_tokenDestroyListener);
    // The token resource is gone. Null our handle so a later commit() does not
    // cache under a recycled wl_resource address. Remove the cache entry if
    // the rect was committed but not yet consumed.
    if (self->m_manager && self->m_committed && !self->m_consumed)
        self->m_manager->m_committedRects.remove(self->m_tokenResource);
    self->m_tokenResource = nullptr;
    wl_list_remove(&self->m_tokenDestroyListener.link);
    wl_list_init(&self->m_tokenDestroyListener.link);
}

void WindowAnimationRectV1::set_geometry(Resource *resource, int32_t x, int32_t y, int32_t width, int32_t height)
{
    if (width <= 0 || height <= 0) {
        wl_resource_post_error(resource->handle,
                               error_invalid_geometry,
                               "set_geometry with non-positive size %dx%d", width, height);
        return;
    }
    m_rect = QRectF(x, y, width, height);
}

void WindowAnimationRectV1::commit(Resource *resource)
{
    if (!m_rect.isValid()) {
        wl_resource_post_error(resource->handle,
                               error_no_geometry,
                               "commit() called before a valid set_geometry");
        return;
    }
    m_committed = true;

    // If already associated with a target wrapper, push the updated geometry.
    if (m_targetWrapper)
        m_targetWrapper->updateWindowAnimationLocalRect(m_rect);

    // If not yet consumed and the token is still alive, cache this rect
    // object under the token resource for later retrieval.
    if (!m_consumed && m_tokenResource && m_manager)
        m_manager->m_committedRects.insert(m_tokenResource, this);

    qCDebug(lcTlWindowAnimation) << "Window animation rect committed:" << m_rect;
}

// ---------------------------------------------------------------------------
// Public class
// ---------------------------------------------------------------------------

WindowAnimationManagerInterfaceV1::WindowAnimationManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new WindowAnimationManagerInterfaceV1Private())
{
}

WindowAnimationManagerInterfaceV1::~WindowAnimationManagerInterfaceV1() = default;

QByteArrayView WindowAnimationManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

std::optional<QRectF> WindowAnimationManagerInterfaceV1::takeCommittedRect(wl_resource *tokenResource)
{
    if (!tokenResource)
        return std::nullopt;
    auto it = d->m_committedRects.find(tokenResource);
    if (it == d->m_committedRects.end())
        return std::nullopt;
    auto *rectObj = it.value();
    d->m_committedRects.erase(it);
    rectObj->m_consumed = true;
    d->m_pendingRect = rectObj;
    return rectObj->rect();
}

bool WindowAnimationManagerInterfaceV1::associatePendingRect(SurfaceWrapper *targetWrapper,
                                                              SurfaceWrapper *originWrapper)
{
    if (!d->m_pendingRect)
        return false;
    d->m_pendingRect->associate(targetWrapper, originWrapper);
    d->m_pendingRect = nullptr;
    return true;
}

void WindowAnimationManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void WindowAnimationManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *WindowAnimationManagerInterfaceV1::global() const
{
    return d->globalHandle();
}
