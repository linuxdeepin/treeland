// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "windowtransitionmanagerinterfacev1.h"

#include "common/treelandlogging.h"
#include "interfaces/windowtransition.h"
#include "qwayland-server-treeland-window-transition-unstable-v1.h"

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
constexpr int kPendingRectLifetimeMs = 60'000;
} // namespace

class WindowTransitionManagerInterfaceV1Private;

static const wlr_surface_role transitionSourceRole = {
    .name = "treeland_window_transition_source_v1",
    .no_object = true,
    .client_commit = nullptr,
    .commit = nullptr,
    .map = nullptr,
    .unmap = nullptr,
    .destroy = nullptr,
};

class WindowTransitionSourceImpl : public WindowTransitionSource
{
public:
    explicit WindowTransitionSourceImpl(wlr_surface *handle)
        : WindowTransitionSource()
        , m_wrapper(new WSurface(handle))
    {
    }

    ~WindowTransitionSourceImpl() override
    {
        const auto outputs = m_refs.keys();
        for (auto *output : outputs)
            m_wrapper->leaveOutput(output);
        delete m_wrapper;
    }

    WSurface *surface() const override
    {
        return m_wrapper;
    }

    void retain(const QList<WOutput *> &outputs) override
    {
        for (auto *output : outputs) {
            if (++m_refs[output] == 1)
                m_wrapper->enterOutput(output);
        }
        m_wrapper->notifyFrameDone();
    }

    void release(const QList<WOutput *> &outputs) override
    {
        for (auto *output : outputs) {
            auto it = m_refs.find(output);
            if (it == m_refs.end())
                continue;
            if (--it.value() <= 0) {
                m_refs.erase(it);
                m_wrapper->leaveOutput(output);
            }
        }
    }

private:
    WSurface *m_wrapper;
    QHash<WOutput *, int> m_refs;
};

struct SourceSurfaceEntry
{
    WindowTransitionManagerInterfaceV1Private *manager = nullptr;
    wlr_surface *handle = nullptr;
    WindowTransitionSourceImpl *source = nullptr;
    wl_listener destroyListener{ };
};

class WindowTransitionRectV1 : public QtWaylandServer::treeland_window_transition_rect_v1
{
    friend class WindowTransitionManagerInterfaceV1;

public:
    WindowTransitionRectV1(WindowTransitionManagerInterfaceV1Private *manager,
                           const QString &token,
                           wl_resource *resource)
        : QtWaylandServer::treeland_window_transition_rect_v1(resource)
        , m_manager(manager)
        , m_token(token)
        , m_expiry(QDeadlineTimer(kPendingRectLifetimeMs))
    {
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

    void associate(WindowTransitionTarget *target, WindowTransitionTarget *origin)
    {
        m_target = target;
        if (m_target) {
            m_target->setWindowTransitionRect(m_rect, origin);
            m_target->setWindowTransitionSource(m_source.data());
            m_targetTransitionFinishedConnection =
                QObject::connect(m_target.data(),
                                 &WindowTransitionTarget::windowTransitionFinished,
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
    void clearSourceSurface();
    void releaseSourceSurface();

    WindowTransitionManagerInterfaceV1Private *m_manager;
    QString m_token;
    QDeadlineTimer m_expiry;
    QRectF m_rect;
    QMetaObject::Connection m_targetTransitionFinishedConnection;
    QPointer<WindowTransitionTarget> m_target;

    // Owned by the manager, not by this rect; only a weak reference.
    QPointer<WindowTransitionSource> m_source;
};

class WindowTransitionManagerInterfaceV1Private
    : public QtWaylandServer::treeland_window_transition_manager_v1
{
public:
    explicit WindowTransitionManagerInterfaceV1Private(WindowTransitionManagerInterfaceV1 *q)
        : QtWaylandServer::treeland_window_transition_manager_v1()
        , q(q)
    {
    }

    ~WindowTransitionManagerInterfaceV1Private();

    wl_global *globalHandle() const
    {
        return m_global;
    }

    // Returns the shared handle for a transition source surface, creating and
    // taking ownership of it on first use.
    WindowTransitionSource *acquireSourceSurface(wlr_surface *surface);

    WindowTransitionManagerInterfaceV1 *q;
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
                                    const QString &token) override
    {
        auto *rectResource = wl_resource_create(resource->client(),
                                                &treeland_window_transition_rect_v1_interface,
                                                resource->version(),
                                                rect_id);
        if (!rectResource) {
            wl_resource_post_no_memory(resource->handle);
            return;
        }
        auto *rect = new WindowTransitionRectV1(this, token, rectResource);
        // One rectangle per token: close an earlier one so the client can
        // release it.
        if (auto *previous = m_pendingRects.take(token))
            previous->notifyDiscarded();
        m_pendingRects.insert(token, rect);
        q->ensurePendingSweepRunning();
    }
};

namespace {
void sourceSurfaceDestroyed(wl_listener *listener, void * /*data*/)
{
    SourceSurfaceEntry *entry;
    entry = wl_container_of(listener, entry, destroyListener);
    wl_list_remove(&entry->destroyListener.link);
    entry->manager->m_sourceSurfaces.remove(entry->handle);
    delete entry->source;
    delete entry;
}
} // namespace

WindowTransitionManagerInterfaceV1Private::~WindowTransitionManagerInterfaceV1Private()
{
    const auto entries = m_sourceSurfaces;
    m_sourceSurfaces.clear();
    for (auto *entry : entries) {
        wl_list_remove(&entry->destroyListener.link);
        delete entry->source;
        delete entry;
    }
}

WindowTransitionSource *WindowTransitionManagerInterfaceV1Private::acquireSourceSurface(
    wlr_surface *surface)
{
    if (auto *entry = m_sourceSurfaces.value(surface))
        return entry->source;

    auto *entry = new SourceSurfaceEntry;
    entry->manager = this;
    entry->handle = surface;
    entry->source = new WindowTransitionSourceImpl(surface);
    entry->destroyListener.notify = sourceSurfaceDestroyed;
    wl_signal_add(&surface->events.destroy, &entry->destroyListener);
    m_sourceSurfaces.insert(surface, entry);
    return entry->source;
}

WindowTransitionRectV1::~WindowTransitionRectV1()
{
    if (m_targetTransitionFinishedConnection)
        QObject::disconnect(m_targetTransitionFinishedConnection);

    if (m_target) {
        m_target->resetWindowTransition();
    }

    releaseSourceSurface();

    if (m_manager && m_manager->m_pendingRects.value(m_token) == this)
        m_manager->m_pendingRects.remove(m_token);
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

    if (m_target)
        m_target->updateWindowTransitionRect(m_rect);

    if (m_source)
        m_source->surface()->notifyFrameDone();
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

    if (m_source && m_source->surface()->handle() == surface)
        return;

    // The surface must not already carry a role.
    if (!wlr_surface_set_role(surface,
                              &transitionSourceRole,
                              resource->handle,
                              error_invalid_surface)) {
        return;
    }

    releaseSourceSurface();

    auto *source = m_manager->acquireSourceSurface(surface);
    m_source = source;

    if (m_target)
        m_target->setWindowTransitionSource(source);

    source->surface()->notifyFrameDone();
}

void WindowTransitionRectV1::clearSourceSurface()
{
    releaseSourceSurface();
    if (m_target)
        m_target->clearWindowTransitionSource();
}

void WindowTransitionRectV1::releaseSourceSurface()
{
    // The handle is owned by the manager; only drop our weak reference.
    m_source = nullptr;
}

WindowTransitionManagerInterfaceV1::WindowTransitionManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(new WindowTransitionManagerInterfaceV1Private(this))
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

bool WindowTransitionManagerInterfaceV1::hasPendingWindowTransitionRect(const QString &token) const
{
    return d->m_pendingRects.contains(token);
}

bool WindowTransitionManagerInterfaceV1::associatePendingRect(const QString &token,
                                                              WindowTransitionTarget *target,
                                                              WindowTransitionTarget *origin)
{
    auto it = d->m_pendingRects.find(token);
    if (it == d->m_pendingRects.end())
        return false;
    auto *rectObj = it.value();
    d->m_pendingRects.erase(it);
    rectObj->associate(target, origin);
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
