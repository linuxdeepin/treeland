// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "windowtransitionmanagerinterfacev1.h"

#include "common/treelandlogging.h"
#include "modules/activation/wayland-xdg-activation-v1-server-protocol.h"
#include "qwayland-server-treeland-window-transition-unstable-v1.h"
#include "surface/surfacewrapper.h"

#include <wayland-server-core.h>
#include <wlr/types/wlr_buffer.h>
#include <wserver.h>
#include <wtools.h>

#include <QDeadlineTimer>
#include <QHash>
#include <QPointer>
#include <QRectF>
#include <QTimer>

#include <cstring>

WAYLIB_SERVER_USE_NAMESPACE
using namespace Qt::StringLiterals;

namespace {
// A consumed transition rect stays pending for the same lifetime as an
// activation token; sweep it once that lifetime elapses so a token that is
// committed but never activated does not leak the rect object.
constexpr int kPendingRectLifetimeMs = 60'000;
}

class WindowTransitionManagerInterfaceV1Private;

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
            m_targetWrapper->setWindowTransitionSourceImage(m_sourceImage);

            m_targetDestroyedConnection =
                QObject::connect(m_targetWrapper.data(), &QObject::destroyed, [this] {
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
    void set_source_buffer(Resource * /*resource*/, struct ::wl_resource *buffer) override;

private:
    static void tokenResourceDestroyed(wl_listener *listener, void *data);

    bool applySourceBuffer(Resource *resource, struct ::wl_resource *buffer);

    WindowTransitionManagerInterfaceV1Private *m_manager;
    wl_resource *m_tokenResource;
    QString m_token;
    QDeadlineTimer m_expiry;
    wl_listener m_tokenDestroyListener;
    QRectF m_rect;
    QImage m_sourceImage;
    uint m_geometrySet : 1 = 0;
    uint m_consumed : 1 = 0;
    QMetaObject::Connection
        m_targetDestroyedConnection;
    QPointer<SurfaceWrapper> m_targetWrapper;
};

class WindowTransitionManagerInterfaceV1Private
    : public QtWaylandServer::treeland_window_transition_manager_v1
{
public:
    explicit WindowTransitionManagerInterfaceV1Private()
        : QtWaylandServer::treeland_window_transition_manager_v1()
    {
    }

    wl_global *globalHandle() const
    {
        return m_global;
    }

    QHash<wl_resource *, WindowTransitionRectV1 *> m_committedRects;
    QHash<QString, WindowTransitionRectV1 *> m_pendingRects;

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

WindowTransitionRectV1::~WindowTransitionRectV1()
{
    if (m_targetDestroyedConnection)
        QObject::disconnect(m_targetDestroyedConnection);

    if (m_targetWrapper) {
        m_targetWrapper->clearWindowTransitionRect();
        m_targetWrapper->clearWindowTransitionSourceImage();
    }

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
}

void WindowTransitionRectV1::set_source_buffer(Resource *resource, struct ::wl_resource *buffer)
{
    if (!buffer) {
        m_sourceImage = QImage();
        if (m_targetWrapper)
            m_targetWrapper->setWindowTransitionSourceImage(m_sourceImage);
        return;
    }
    applySourceBuffer(resource, buffer);
}

bool WindowTransitionRectV1::applySourceBuffer(Resource *resource,
                                               struct ::wl_resource *bufferResource)
{
    wlr_buffer *buffer = wlr_buffer_try_from_resource(bufferResource);
    if (!buffer) {
        wl_resource_post_error(resource->handle,
                               error_invalid_buffer,
                               "set_source_buffer: buffer could not be imported");
        return false;
    }

    void *data = nullptr;
    uint32_t format = 0;
    size_t stride = 0;
    if (!wlr_buffer_begin_data_ptr_access(buffer,
                                          WLR_BUFFER_DATA_PTR_ACCESS_READ,
                                          &data,
                                          &format,
                                          &stride)) {
        wl_resource_post_error(resource->handle,
                               error_invalid_buffer,
                               "set_source_buffer: buffer content is not CPU-readable");
        wlr_buffer_unlock(buffer);
        return false;
    }

    const QImage::Format qFormat = WTools::toImageFormat(format);
    if (qFormat == QImage::Format_Invalid) {
        wl_resource_post_error(resource->handle,
                               error_invalid_buffer,
                               "set_source_buffer: unsupported buffer format 0x%x",
                               format);
        wlr_buffer_end_data_ptr_access(buffer);
        wlr_buffer_unlock(buffer);
        return false;
    }

    QImage image(buffer->width, buffer->height, qFormat);
    if (!image.isNull() && image.bytesPerLine() == static_cast<int>(stride)) {
        memcpy(image.bits(), data, static_cast<size_t>(buffer->height) * stride);
    } else if (!image.isNull()) {
        // Stride mismatch (e.g. row padding): copy row by row.
        const int copyBytes = qMin<size_t>(image.bytesPerLine(), stride);
        for (int y = 0; y < buffer->height; ++y) {
            memcpy(image.scanLine(y),
                   static_cast<const uchar *>(data) + static_cast<size_t>(y) * stride,
                   copyBytes);
        }
    }
    wlr_buffer_end_data_ptr_access(buffer);
    wlr_buffer_unlock(buffer);

    if (image.isNull()) {
        wl_resource_post_error(resource->handle,
                               error_invalid_buffer,
                               "set_source_buffer: failed to allocate image");
        return false;
    }

    m_sourceImage = image;
    if (m_targetWrapper)
        m_targetWrapper->setWindowTransitionSourceImage(m_sourceImage);
    return true;
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
