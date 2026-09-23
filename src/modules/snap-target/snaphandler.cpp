// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "snaphandler.h"

#include "qwayland-server-treeland-snap-target-unstable-v1.h"
#include "surface/surfacewrapper.h"

#include <private/qquickitem_p.h>

#include <wcursor.h>
#include <wlr_all.h>
#include <woutputitem.h>
#include <woutputrenderwindow.h>
#include <wseat.h>
#include <wserver.h>
#include <wsurfaceitem.h>
#include <wtoplevelsurface.h>

#include <QList>
#include <QMetaObject>
#include <QPointF>
#include <QPointer>
#include <QQuickItem>
#include <QRectF>

#include <utility>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
struct SnappableTarget
{
    QRectF rect;
    QPointer<WToplevelSurface> shellSurface;

    bool operator==(const SnappableTarget &other) const
    {
        return rect == other.rect && shellSurface == other.shellSurface;
    }
};

SurfaceWrapper *snapMaskWrapperFor(QQuickItem *item)
{
    for (QQuickItem *parent = item; parent; parent = parent->parentItem()) {
        if (auto *wrapper = qobject_cast<SurfaceWrapper *>(parent))
            return wrapper;
    }
    return nullptr;
}

QList<SnappableTarget> collectSnappableTargets(WOutputRenderWindow *renderWindow)
{
    QList<SnappableTarget> result;
    if (!renderWindow || !renderWindow->contentItem())
        return result;

    QList<QPointer<QQuickItem>> outputItems;

    auto items =
        WOutputRenderWindow::paintOrderItemList(renderWindow->contentItem(),
                                                [&outputItems](QQuickItem *item) -> bool {
                                                    if (!item->isVisible())
                                                        return false;
                                                    if (qobject_cast<WOutputItem *>(item)) {
                                                        outputItems.append(item);
                                                        return false;
                                                    }
                                                    if (qobject_cast<WSurfaceItem *>(item))
                                                        return true;
                                                    return false;
                                                });

    for (auto it = items.crbegin(); it != items.crend(); ++it) {
        if (!*it)
            continue;
        auto surfaceItem = qobject_cast<WSurfaceItem *>(*it);
        if (!surfaceItem)
            continue;
        // Snap-mask surfaces are overlay masks and never snap targets.
        if (auto *wrapper = snapMaskWrapperFor(surfaceItem); wrapper && wrapper->isSnapMask())
            continue;
        SnappableTarget target;
        target.rect = surfaceItem->mapRectToScene(surfaceItem->boundingRect());
        target.shellSurface = surfaceItem->shellSurface();
        result.append(target);
    }

    for (const auto &item : std::as_const(outputItems)) {
        if (!item)
            continue;
        SnappableTarget target;
        target.rect = item->mapRectToScene(item->boundingRect());
        result.append(target);
    }

    return result;
}

SnappableTarget hitTestSnappableTarget(const QPointF &cursorPos,
                                       const QList<SnappableTarget> &snapshot)
{
    for (const auto &target : snapshot) {
        if (target.rect.contains(cursorPos))
            return target;
    }
    return { };
}
} // namespace

class SnapTargetV1Private
    : public QObject
    , public QtWaylandServer::treeland_snap_target_v1
{
public:
    explicit SnapTargetV1Private(QObject *parent = nullptr)
        : QObject(parent)
        , QtWaylandServer::treeland_snap_target_v1()
    {
    }

    ~SnapTargetV1Private() override
    {
        stopSnapping();
    }

    wl_global *globalHandle() const
    {
        return m_global;
    }

    WOutputRenderWindow *renderWindow() const
    {
        return m_renderWindow;
    }

    void setRenderWindow(WOutputRenderWindow *renderWindow)
    {
        m_renderWindow = renderWindow;
    }

protected:
    void start(Resource *resource, struct ::wl_resource *seatResource, uint32_t events) override;
    void stop(Resource *resource) override;
    void destroy(Resource *resource) override;
    void destroy_resource(Resource *resource) override;

private:
    void updateSnappableTarget();
    void sendSnappableTarget(const SnappableTarget &target);
    void stopSnapping();

    QPointer<WOutputRenderWindow> m_renderWindow;
    QPointer<WSeat> m_seat;

    Resource *m_activeResource = nullptr;
    bool m_pidfdEnabled = false;
    QMetaObject::Connection m_cursorConn;
    QList<SnappableTarget> m_snapshot;
    SnappableTarget m_lastTarget;
};

void SnapTargetV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void SnapTargetV1Private::destroy_resource(Resource *resource)
{
    if (m_activeResource == resource)
        stopSnapping();
}

void SnapTargetV1Private::start(Resource *resource,
                                struct ::wl_resource *seatResource,
                                uint32_t events)
{
    // Only one snap session may be active across all clients. start on an
    // already active snap object is ignored; start by another client fails
    // with the snap_busy reason.
    if (m_activeResource) {
        if (m_activeResource != resource)
            send_failed(resource->handle, failure_reason_snap_busy);
        return;
    }

    WSeat *seat = nullptr;
    if (seatResource) {
        auto *seatClient = wlr_seat_client_from_resource(seatResource);
        if (seatClient)
            seat = WSeat::fromHandle(seatClient->seat);
    }

    m_activeResource = resource;
    m_seat = seat;
    m_pidfdEnabled = events & event_type_pidfd;

    m_snapshot = collectSnappableTargets(m_renderWindow);

    if (seat && seat->cursor()) {
        m_cursorConn = connect(seat->cursor(),
                               &WCursor::positionChanged,
                               this,
                               &SnapTargetV1Private::updateSnappableTarget);
    }

    updateSnappableTarget();
}

void SnapTargetV1Private::stop(Resource *resource)
{
    if (m_activeResource != resource)
        return;
    stopSnapping();
}

void SnapTargetV1Private::stopSnapping()
{
    if (!m_activeResource)
        return;

    m_activeResource = nullptr;
    m_seat = nullptr;
    m_pidfdEnabled = false;

    if (m_cursorConn) {
        disconnect(m_cursorConn);
        m_cursorConn = QMetaObject::Connection();
    }

    m_snapshot.clear();
    m_lastTarget = SnappableTarget();
}

void SnapTargetV1Private::updateSnappableTarget()
{
    if (!m_activeResource)
        return;

    auto seat = m_seat;
    if (!seat || !seat->cursor())
        return;

    auto cursorPos = seat->cursor()->position();
    auto target = hitTestSnappableTarget(cursorPos, m_snapshot);

    if (target == m_lastTarget)
        return;

    m_lastTarget = target;
    sendSnappableTarget(target);
}

void SnapTargetV1Private::sendSnappableTarget(const SnappableTarget &target)
{
    if (!m_activeResource)
        return;

    // The pidfd event is sent only when the client enabled it and the target
    // has a determinable owning process; it always precedes the snap_region
    // event that commits it.
    if (m_pidfdEnabled && !target.rect.isEmpty() && target.shellSurface) {
        int pidfd = target.shellSurface->pidFD();
        if (pidfd >= 0)
            send_pidfd(m_activeResource->handle, pidfd);
    }

    const QRectF &region = target.rect;
    if (region.isValid() && !region.isEmpty()) {
        send_snap_region(m_activeResource->handle,
                         static_cast<int32_t>(region.x()),
                         static_cast<int32_t>(region.y()),
                         static_cast<uint32_t>(region.width()),
                         static_cast<uint32_t>(region.height()));
    } else {
        send_snap_region(m_activeResource->handle, 0, 0, 0, 0);
    }
}

SnapTargetV1::SnapTargetV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(std::make_unique<SnapTargetV1Private>())
{
}

SnapTargetV1::~SnapTargetV1() = default;

WOutputRenderWindow *SnapTargetV1::outputRenderWindow() const
{
    return d->renderWindow();
}

void SnapTargetV1::setOutputRenderWindow(WOutputRenderWindow *renderWindow)
{
    d->setRenderWindow(renderWindow);
}

QByteArrayView SnapTargetV1::interfaceName() const
{
    return QtWaylandServer::treeland_snap_target_v1::interfaceName();
}

void SnapTargetV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void SnapTargetV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *SnapTargetV1::global() const
{
    return d->globalHandle();
}
