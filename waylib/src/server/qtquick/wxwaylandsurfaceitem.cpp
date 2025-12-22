// Copyright (C) 2023-2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxwaylandsurfaceitem.h"
#include "wsurfaceitem_p.h"
#include "wxwaylandsurface.h"
#include "wxwayland.h"

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXWaylandSurfaceItemPrivate : public WSurfaceItemPrivate
{
    Q_DECLARE_PUBLIC(WXWaylandSurfaceItem)
public:
    void configureSurface(const QRect &newGeometry);
    QSize expectSurfaceSize() const;
    QPoint explicitSurfacePosition() const;
    static inline WXWaylandSurfaceItemPrivate *get(WXWaylandSurfaceItem *qq) {
        return qq->d_func();
    }

public:
    QPointF surfacePosition;
    bool positionConfigured = false;

    QPointer<WXWaylandSurfaceItem> parentSurfaceItem;
    QSize minimumSize;
    QSize maximumSize;
};

void WXWaylandSurfaceItemPrivate::configureSurface(const QRect &newGeometry)
{
    Q_Q(WXWaylandSurfaceItem);
    if (!q->isVisible())
        return;
    q->xwaylandSurface()->configure(newGeometry);
    q->updateSurfaceState();
}

QSize WXWaylandSurfaceItemPrivate::expectSurfaceSize() const
{
    const Q_Q(WXWaylandSurfaceItem);
    using enum WXWaylandSurfaceItem::ResizeMode;

    if (q->resizeMode() == SizeFromSurface) {
        const bool useRequestSize = !q->xwaylandSurface()->isBypassManager()
            && q->xwaylandSurface()->requestConfigureFlags()
                   .testAnyFlags(WXWaylandSurface::XCB_CONFIG_WINDOW_SIZE);
        return useRequestSize
            ? q->xwaylandSurface()->requestConfigureGeometry().size()
            : q->xwaylandSurface()->geometry().size();
    } else if (q->resizeMode() == SizeToSurface) {
        const auto s = (q->size() - paddingsSize()) * surfaceSizeRatio;
        return s.toSize();
    }

    return q->xwaylandSurface()->geometry().size();
}

QPoint WXWaylandSurfaceItemPrivate::explicitSurfacePosition() const
{
    const Q_Q(WXWaylandSurfaceItem);

    const QPointF pos = surfacePosition;
    const qreal ssr = surfaceSizeRatio;

    return (pos * ssr + QPointF(q->leftPadding(), q->topPadding()) * ssr).toPoint();
}

WXWaylandSurfaceItem::WXWaylandSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(*new WXWaylandSurfaceItemPrivate(), parent)
{

}

WXWaylandSurfaceItem::~WXWaylandSurfaceItem()
{

}

WXWaylandSurface *WXWaylandSurfaceItem::xwaylandSurface() const
{
    return qobject_cast<WXWaylandSurface*>(shellSurface());
}

bool WXWaylandSurfaceItem::setShellSurface(WToplevelSurface *surface)
{
    if (!WSurfaceItem::setShellSurface(surface))
        return false;

    if (surface) {
        Q_ASSERT(surface->surface());
        surface->safeConnect(&WXWaylandSurface::surfaceChanged, this, [this] {
            WSurfaceItem::setSurface(xwaylandSurface()->surface());
        });

        auto updateGeometry = [this] {
            const auto rm = resizeMode();
            if (rm != SizeFromSurface)
                return;
            if (!isVisible())
                return;

            updateSurfaceState();

            if (rm == SizeFromSurface) {
                resize(rm);
            }
        };
        xwaylandSurface()->safeConnect(&WXWaylandSurface::requestConfigure, this, [this] {
            if (xwaylandSurface()->requestConfigureFlags().testAnyFlags(
                    WXWaylandSurface::ConfigureFlag::XCB_CONFIG_WINDOW_POSITION)) {
                Q_EMIT implicitPositionChanged();
            }
        });
        xwaylandSurface()->safeConnect(&WXWaylandSurface::geometryChanged, this, updateGeometry);
        connect(this, &WXWaylandSurfaceItem::topPaddingChanged,
               this, &WXWaylandSurfaceItem::updatePosition, Qt::UniqueConnection);
        connect(this, &WXWaylandSurfaceItem::leftPaddingChanged,
               this, &WXWaylandSurfaceItem::updatePosition, Qt::UniqueConnection);
    }
    return true;
}

WXWaylandSurfaceItem *WXWaylandSurfaceItem::parentSurfaceItem() const
{
    const Q_D(WXWaylandSurfaceItem);

    return d->parentSurfaceItem;
}

void WXWaylandSurfaceItem::setParentSurfaceItem(WXWaylandSurfaceItem *newParentSurfaceItem)
{
    Q_D(WXWaylandSurfaceItem);

    if (d->parentSurfaceItem == newParentSurfaceItem)
        return;
    if (d->parentSurfaceItem) {
        d->parentSurfaceItem->disconnect(this);
    }

    d->parentSurfaceItem = newParentSurfaceItem;
    Q_EMIT parentSurfaceItemChanged();

    updatePosition();
}

QSize WXWaylandSurfaceItem::maximumSize() const
{
    const Q_D(WXWaylandSurfaceItem);

    return d->maximumSize;
}

QSize WXWaylandSurfaceItem::minimumSize() const
{
    const Q_D(WXWaylandSurfaceItem);

    return d->minimumSize;
}

void WXWaylandSurfaceItem::moveTo(const QPointF &pos, bool configSurface)
{
    Q_D(WXWaylandSurfaceItem);
    if (d->surfacePosition == pos)
        return;
    d->surfacePosition = pos;

    if (configSurface) {
        updatePosition();
        d->positionConfigured = true;
    }
}

QPointF WXWaylandSurfaceItem::implicitPosition() const
{
    const Q_D(WXWaylandSurfaceItem);

    auto xwaylandSurface = qobject_cast<WXWaylandSurface *>(d->shellSurface);

    const QPoint epos = xwaylandSurface->requestConfigureGeometry().topLeft();
    const qreal ssr = d->surfaceSizeRatio;

    return QPointF(epos) / ssr - QPointF(leftPadding(), topPadding());
}


void WXWaylandSurfaceItem::onSurfaceCommit()
{
    Q_D(WXWaylandSurfaceItem);
    WSurfaceItem::onSurfaceCommit();

    QSize minSize = xwaylandSurface()->minSize();
    if (!minSize.isValid())
        minSize = QSize(0, 0);

    QSize maxSize = xwaylandSurface()->maxSize();
    if (maxSize.isValid())
        maxSize = QSize(INT_MAX, INT_MAX);

    if (d->minimumSize != minSize) {
        d->minimumSize = minSize;
        Q_EMIT minimumSizeChanged();
    }

    if (d->maximumSize != maxSize) {
        d->maximumSize = maxSize;
        Q_EMIT maximumSizeChanged();
    }
}

void WXWaylandSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(xwaylandSurface());
    connect(xwaylandSurface(), &WWrapObject::aboutToBeInvalidated,
            this, &WXWaylandSurfaceItem::releaseResources);
}

bool WXWaylandSurfaceItem::doResizeSurface(const QSize &newSize)
{
    Q_D(WXWaylandSurfaceItem);
    d->configureSurface(QRect(d->explicitSurfacePosition(), newSize));
    return true;
}

QRectF WXWaylandSurfaceItem::getContentGeometry() const
{
    return xwaylandSurface()->getContentGeometry();
}

QSizeF WXWaylandSurfaceItem::getContentSize() const
{
    return getContentGeometry().size();
}

void WXWaylandSurfaceItem::surfaceSizeRatioChange()
{
    WSurfaceItem::surfaceSizeRatioChange();

    W_DC(WXWaylandSurfaceItem);

    if (d->positionConfigured) {
        updatePosition();
    } else {
        Q_EMIT implicitPositionChanged();
    }
}

void WXWaylandSurfaceItem::updatePosition()
{
    Q_D(WXWaylandSurfaceItem);
    d->configureSurface(QRect(d->explicitSurfacePosition(),
                              d->expectSurfaceSize()));
}

WAYLIB_SERVER_END_NAMESPACE
