// Copyright (C) 2023-2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxwaylandsurfaceitem.h"
#include "wsurfaceitem_p.h"
#include "wxwaylandsurface.h"
#include "wxwayland.h"

#include <QQmlInfo>
#include <private/qquickitem_p.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXWaylandSurfaceItemPrivate : public WSurfaceItemPrivate
{
    Q_DECLARE_PUBLIC(WXWaylandSurfaceItem)
public:
    void setImplicitPosition(const QPointF &newImplicitPosition);
    void checkMove(WXWaylandSurfaceItem::PositionMode mode);
    void doMove(WXWaylandSurfaceItem::PositionMode mode);
    void configureSurface(const QRect &newGeometry);
    // get xwayland surface's position of specified mode
    QPoint expectSurfacePosition(WXWaylandSurfaceItem::PositionMode mode) const;
    // get xwayland surface's size of specified mode
    QSize expectSurfaceSize(WXWaylandSurfaceItem::ResizeMode mode) const;
    static inline WXWaylandSurfaceItemPrivate *get(WXWaylandSurfaceItem *qq) {
        return qq->d_func();
    }

public:
    QPointer<WXWaylandSurfaceItem> parentSurfaceItem;
    QSize minimumSize;
    QSize maximumSize;
    WXWaylandSurfaceItem::PositionMode positionMode = WXWaylandSurfaceItem::PositionFromSurface;
    QPointF positionOffset;
    bool ignoreConfigureRequest = false;
};

void WXWaylandSurfaceItemPrivate::checkMove(WXWaylandSurfaceItem::PositionMode mode)
{
    Q_Q(WXWaylandSurfaceItem);
    using enum WXWaylandSurfaceItem::PositionMode;

    if (!q->xwaylandSurface() || mode == ManualPosition || !q->isVisible())
        return;
    doMove(mode);
}

void WXWaylandSurfaceItemPrivate::doMove(WXWaylandSurfaceItem::PositionMode mode)
{
    Q_Q(WXWaylandSurfaceItem);
    using enum WXWaylandSurfaceItem::PositionMode;
    Q_ASSERT(mode != WXWaylandSurfaceItem::ManualPosition);
    Q_ASSERT(q->isVisible());

    if (mode == PositionFromSurface) {
        const QPoint epos = expectSurfacePosition(mode);
        QPointF pos = epos;

        const qreal ssr = parentSurfaceItem ? parentSurfaceItem->surfaceSizeRatio() : 1.0;
        const auto pt = q->parentItem();
        if (pt && !qFuzzyCompare(ssr, 1.0)) {
            auto *pd = WXWaylandSurfaceItemPrivate::get(parentSurfaceItem);
            const QPoint pepos = pd->expectSurfacePosition(mode);
            pos = pepos + (epos - pepos) / ssr;
        }

        q->setPosition(pos - positionOffset);
    } else if (mode == PositionToSurface) {
        configureSurface(QRect(expectSurfacePosition(mode), expectSurfaceSize(q->resizeMode())));
    }
}

void WXWaylandSurfaceItemPrivate::configureSurface(const QRect &newGeometry)
{
    Q_Q(WXWaylandSurfaceItem);
    if (!q->isVisible())
        return;
    q->xwaylandSurface()->configure(newGeometry);
    q->updateSurfaceState();
}

QSize WXWaylandSurfaceItemPrivate::expectSurfaceSize(WXWaylandSurfaceItem::ResizeMode mode) const
{
    const Q_Q(WXWaylandSurfaceItem);
    using enum WXWaylandSurfaceItem::ResizeMode;

    if (mode == SizeFromSurface) {
        const bool useRequestSize = !q->xwaylandSurface()->isBypassManager()
            && q->xwaylandSurface()->requestConfigureFlags()
                   .testAnyFlags(WXWaylandSurface::XCB_CONFIG_WINDOW_SIZE);
        return useRequestSize
            ? q->xwaylandSurface()->requestConfigureGeometry().size()
            : q->xwaylandSurface()->geometry().size();
    } else if (mode == SizeToSurface) {
        return q->getContentSize().toSize();
    }

    return q->xwaylandSurface()->geometry().size();
}

QPoint WXWaylandSurfaceItemPrivate::expectSurfacePosition(WXWaylandSurfaceItem::PositionMode mode) const
{
    const Q_Q(WXWaylandSurfaceItem);
    using enum WXWaylandSurfaceItem::PositionMode;

    if (mode == PositionFromSurface) {
        const bool useRequestPositon = !q->xwaylandSurface()->isBypassManager()
            && q->xwaylandSurface()->requestConfigureFlags()
                   .testAnyFlags(WXWaylandSurface::XCB_CONFIG_WINDOW_POSITION);
        return useRequestPositon
            ? q->xwaylandSurface()->requestConfigureGeometry().topLeft()
            : q->xwaylandSurface()->geometry().topLeft();
    } else if (mode == PositionToSurface) {
        QPointF pos = q->position();
        const qreal ssr = parentSurfaceItem ? parentSurfaceItem->surfaceSizeRatio() : 1.0;
        const auto pt = q->parentItem();
        if (pt && !qFuzzyCompare(ssr, 1.0)) {
            const QPointF poffset(parentSurfaceItem->leftPadding(), parentSurfaceItem->topPadding());
            pos = pt->mapToItem(parentSurfaceItem, pos) - poffset;
            pos = pt->mapFromItem(parentSurfaceItem, pos * ssr + poffset);
        }

        return (pos + positionOffset + QPointF(q->leftPadding(), q->topPadding())).toPoint();
    }

    return q->xwaylandSurface()->geometry().topLeft();
}

WXWaylandSurfaceItem::WXWaylandSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(*new WXWaylandSurfaceItemPrivate(), parent)
{

}

WXWaylandSurfaceItem::~WXWaylandSurfaceItem()
{

}

bool WXWaylandSurfaceItem::setShellSurface(WToplevelSurface *surface)
{
    Q_D(WXWaylandSurfaceItem);

    if (!WSurfaceItem::setShellSurface(surface))
        return false;

    if (surface) {
        Q_ASSERT(surface->surface());
        surface->safeConnect(&WXWaylandSurface::surfaceChanged, this, [this] {
            WSurfaceItem::setSurface(xwaylandSurface()->surface());
        });

        auto updateGeometry = [this, d] {
            const auto rm = resizeMode();
            if (rm != SizeFromSurface && d->positionMode != PositionFromSurface)
                return;
            if (!isVisible())
                return;

            updateSurfaceState();

            if (rm == SizeFromSurface) {
                resize(rm);
            }
            if (d->positionMode == PositionFromSurface) {
                d->doMove(d->positionMode);
            }
        };

        xwaylandSurface()->safeConnect(&WXWaylandSurface::requestConfigure,
                this, [updateGeometry, this, d] {
            if (d->ignoreConfigureRequest)
                return;
            const QRect geometry(d->expectSurfacePosition(d->positionMode),
                                 d->expectSurfaceSize(resizeMode()));
            d->configureSurface(geometry);
            updateGeometry();
        });
        xwaylandSurface()->safeConnect(&WXWaylandSurface::geometryChanged, this, updateGeometry);
        connect(this, &WXWaylandSurfaceItem::topPaddingChanged,
                this, &WXWaylandSurfaceItem::updatePosition, Qt::UniqueConnection);
        connect(this, &WXWaylandSurfaceItem::leftPaddingChanged,
                this, &WXWaylandSurfaceItem::updatePosition, Qt::UniqueConnection);
        // TODO: Maybe we shouldn't think about the effectiveVisible for surface/item's position
        // This behovior can control by compositor using PositionMode::ManualPosition
        connect(this, &WXWaylandSurfaceItem::effectiveVisibleChanged,
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

    if (d->parentSurfaceItem)
        connect(d->parentSurfaceItem, &WSurfaceItem::surfaceSizeRatioChanged, this, &WXWaylandSurfaceItem::updatePosition);
    d->checkMove(d->positionMode);
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

WXWaylandSurfaceItem::PositionMode WXWaylandSurfaceItem::positionMode() const
{
    const Q_D(WXWaylandSurfaceItem);

    return d->positionMode;
}

void WXWaylandSurfaceItem::setPositionMode(PositionMode newPositionMode)
{
    Q_D(WXWaylandSurfaceItem);

    if (d->positionMode == newPositionMode)
        return;
    d->positionMode = newPositionMode;
    Q_EMIT positionModeChanged();
}

void WXWaylandSurfaceItem::move(PositionMode mode)
{
    Q_D(WXWaylandSurfaceItem);

    if (mode == ManualPosition) {
        qmlWarning(this) << "Can't move WXWaylandSurfaceItem for ManualPosition mode.";
        return;
    }

    if (!isVisible())
        return;

    d->doMove(mode);
}

QPointF WXWaylandSurfaceItem::positionOffset() const
{
    const Q_D(WXWaylandSurfaceItem);

    return d->positionOffset;
}

void WXWaylandSurfaceItem::setPositionOffset(QPointF newPositionOffset)
{
    Q_D(WXWaylandSurfaceItem);
    if (d->positionOffset == newPositionOffset)
        return;
    d->positionOffset = newPositionOffset;
    Q_EMIT positionOffsetChanged();

    updatePosition();
}

bool WXWaylandSurfaceItem::ignoreConfigureRequest() const
{
    const Q_D(WXWaylandSurfaceItem);

    return d->ignoreConfigureRequest;
}

void WXWaylandSurfaceItem::setIgnoreConfigureRequest(bool newIgnoreConfigureRequest)
{
    Q_D(WXWaylandSurfaceItem);

    if (d->ignoreConfigureRequest == newIgnoreConfigureRequest)
        return;
    d->ignoreConfigureRequest = newIgnoreConfigureRequest;
    Q_EMIT ignoreConfigureRequestChanged();
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
    Q_D(WXWaylandSurfaceItem);
    WSurfaceItem::initSurface();
    Q_ASSERT(xwaylandSurface());
    connect(xwaylandSurface(), &WWrapObject::aboutToBeInvalidated,
            this, &WXWaylandSurfaceItem::releaseResources);
    updatePosition();
}

bool WXWaylandSurfaceItem::doResizeSurface(const QSize &newSize)
{
    Q_D(WXWaylandSurfaceItem);
    d->configureSurface(QRect(d->expectSurfacePosition(d->positionMode), newSize));
    return true;
}

QRectF WXWaylandSurfaceItem::getContentGeometry() const
{
    return xwaylandSurface()->getContentGeometry();
}

QSizeF WXWaylandSurfaceItem::getContentSize() const
{
    return (size() - QSizeF(leftPadding() + rightPadding(), topPadding() + bottomPadding())) * surfaceSizeRatio();
}

void WXWaylandSurfaceItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_D(WXWaylandSurfaceItem);
    WSurfaceItem::geometryChange(newGeometry, oldGeometry);

    if (newGeometry.topLeft() != oldGeometry.topLeft()
        && d->positionMode == PositionToSurface && xwaylandSurface()) {
        d->configureSurface(QRect(d->expectSurfacePosition(d->positionMode),
                                  d->expectSurfaceSize(resizeMode())));
    }
}

void WXWaylandSurfaceItem::updatePosition()
{
    Q_D(WXWaylandSurfaceItem);
    d->checkMove(d->positionMode);
}

WAYLIB_SERVER_END_NAMESPACE
