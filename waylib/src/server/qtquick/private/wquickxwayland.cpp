// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickxwayland_p.h"
#include "wquickwaylandserver.h"
#include "wseat.h"
#include "wxwaylandsurface.h"
#include "wxwayland.h"

#include <qwxwayland.h>
#include <qwxwaylandsurface.h>
#include <qwxwaylandshellv1.h>

#include <QQmlInfo>
#include <private/qquickitem_p.h>

extern "C" {
#define class className
#include <wlr/xwayland.h>
#undef class
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

WXWaylandShellV1::WXWaylandShellV1(QObject *parent)
    : WQuickWaylandServerInterface(parent)
{

}

QWXWaylandShellV1 *WXWaylandShellV1::shell() const
{
    return m_shell;
}

void WXWaylandShellV1::create()
{
    m_shell = QWXWaylandShellV1::create(server()->handle(), 1);
    if (!m_shell)
        Q_EMIT shellChanged();
}

class XWayland : public WXWayland
{
public:
    XWayland(WQuickXWayland *qq)
        : WXWayland(qq->compositor(), qq->lazy())
        , qq(qq)
    {

    }

    void surfaceAdded(WXWaylandSurface *surface) override;
    void surfaceRemoved(WXWaylandSurface *surface) override;
    void create(WServer *server) override;

    WQuickXWayland *qq;
};

void XWayland::surfaceAdded(WXWaylandSurface *surface)
{
    WXWayland::surfaceAdded(surface);

    QObject::connect(surface, &WXWaylandSurface::isToplevelChanged,
                     qq, &WQuickXWayland::onIsToplevelChanged);
    QObject::connect(surface->handle(), &QWXWaylandSurface::associate,
                     qq, [this, surface] {
        qq->addSurface(surface);
    });
    QObject::connect(surface->handle(), &QWXWaylandSurface::dissociate,
                     qq, [this, surface] {
        qq->removeSurface(surface);
    });

    if (surface->surface())
        qq->addSurface(surface);
}

void XWayland::surfaceRemoved(WXWaylandSurface *surface)
{
    WXWayland::surfaceRemoved(surface);
    Q_EMIT qq->surfaceRemoved(surface);

    qq->removeToplevel(surface);
}

void XWayland::create(WServer *server)
{
    WXWayland::create(server);
    setSeat(qq->seat());
}

WQuickXWayland::WQuickXWayland(QObject *parent)
    : WQuickWaylandServerInterface(parent)
{

}

bool WQuickXWayland::lazy() const
{
    return m_lazy;
}

void WQuickXWayland::setLazy(bool newLazy)
{
    if (m_lazy == newLazy)
        return;

    if (xwayland && xwayland->isValid()) {
        qmlWarning(this) << "Can't change \"lazy\" after xwayland created";
        return;
    }

    m_lazy = newLazy;
    Q_EMIT lazyChanged();
}

QWCompositor *WQuickXWayland::compositor() const
{
    return m_compositor;
}

void WQuickXWayland::setCompositor(QWLRoots::QWCompositor *compositor)
{
    if (m_compositor == compositor)
        return;

    if (xwayland && xwayland->isValid()) {
        qmlWarning(this) << "Can't change \"compositor\" after xwayland created";
        return;
    }

    m_compositor = compositor;
    Q_EMIT compositorChanged();

    if (isPolished())
        tryCreateXWayland();
}

wl_client *WQuickXWayland::client() const
{
    if (!xwayland || !xwayland->isValid())
        return nullptr;
    return xwayland->handle()->handle()->server->client;
}

pid_t WQuickXWayland::pid() const
{
    if (!xwayland || !xwayland->isValid())
        return 0;
    return xwayland->handle()->handle()->server->pid;
}

QByteArray WQuickXWayland::displayName() const
{
    if (!xwayland)
        return {};
    return xwayland->displayName();
}

WSeat *WQuickXWayland::seat() const
{
    return m_seat;
}

void WQuickXWayland::setSeat(WSeat *newSeat)
{
    if (m_seat == newSeat)
        return;
    m_seat = newSeat;

    if (xwayland && xwayland->isValid())
        xwayland->setSeat(m_seat);

    Q_EMIT seatChanged();
}

void WQuickXWayland::create()
{
    WQuickWaylandServerInterface::create();
    tryCreateXWayland();
}

void WQuickXWayland::tryCreateXWayland()
{
    if (!m_compositor)
        return;

    Q_ASSERT(!xwayland);

    xwayland = server()->attach<XWayland>(this);
    connect(xwayland->handle(), &QWXWayland::ready, this, &WQuickXWayland::ready);

    Q_EMIT displayNameChanged();
}

void WQuickXWayland::onIsToplevelChanged()
{
    auto surface = qobject_cast<WXWaylandSurface*>(sender());
    Q_ASSERT(surface);

    if (!surface->surface())
        return;

    if (surface->isToplevel()) {
        addToplevel(surface);
    } else {
        removeToplevel(surface);
    }
}

void WQuickXWayland::addSurface(WXWaylandSurface *surface)
{
    Q_EMIT surfaceAdded(surface);

    if (surface->isToplevel())
        addToplevel(surface);
}

void WQuickXWayland::removeSurface(WXWaylandSurface *surface)
{
    Q_EMIT surfaceRemoved(surface);

    if (surface->isToplevel())
        removeToplevel(surface);
}

void WQuickXWayland::addToplevel(WXWaylandSurface *surface)
{
    if (toplevelSurfaces.contains(surface))
        return;
    toplevelSurfaces.append(surface);
    Q_EMIT toplevelAdded(surface);
}

void WQuickXWayland::removeToplevel(WXWaylandSurface *surface)
{
    if (toplevelSurfaces.removeOne(surface))
        Q_EMIT toplevelRemoved(surface);
}

WXWaylandSurfaceItem::WXWaylandSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(parent)
{

}

WXWaylandSurfaceItem::~WXWaylandSurfaceItem()
{

}

WXWaylandSurface *WXWaylandSurfaceItem::surface() const
{
    return m_surface;
}

void WXWaylandSurfaceItem::setSurface(WXWaylandSurface *surface)
{
    if (m_surface == surface)
        return;

    if (m_surface) {
        bool ok = m_surface->disconnect(this);
        Q_ASSERT(ok);
    }

    m_surface = surface;

    if (surface) {
        Q_ASSERT(surface->surface());
        WSurfaceItem::setSurface(surface->surface());
        connect(surface, &WXWaylandSurface::surfaceChanged, this, [this] {
            WSurfaceItem::setSurface(m_surface->surface());
        });

        auto updateGeometry = [this] {
            const auto rm = resizeMode();
            if (rm != SizeFromSurface && m_positionMode != PositionFromSurface)
                return;
            if (!isVisible())
                return;

            updateSurfaceState();

            if (rm == SizeFromSurface) {
                resize(rm);
            }
            if (m_positionMode == PositionFromSurface) {
                move(m_positionMode);
            }
        };

        connect(surface, &WXWaylandSurface::requestConfigure,
                this, [updateGeometry, this] {
            if (m_ignoreConfigureRequest)
                return;
            const QRect geometry(expectSurfacePosition(m_positionMode),
                                 expectSurfaceSize(resizeMode()));
            configureSurface(geometry);
            updateGeometry();
        });
        connect(surface, &WXWaylandSurface::geometryChanged, this, updateGeometry);
        connect(this, &WXWaylandSurfaceItem::topPaddingChanged,
                this, &WXWaylandSurfaceItem::updatePosition, Qt::UniqueConnection);
        connect(this, &WXWaylandSurfaceItem::leftPaddingChanged,
                this, &WXWaylandSurfaceItem::updatePosition, Qt::UniqueConnection);
        // TODO: Maybe we shouldn't think about the effectiveVisible for surface/item's position
        // This behovior can control by compositor using PositionMode::ManualPosition
        connect(this, &WXWaylandSurfaceItem::effectiveVisibleChanged,
                this, &WXWaylandSurfaceItem::updatePosition, Qt::UniqueConnection);
    } else {
        WSurfaceItem::setSurface(nullptr);
    }

    Q_EMIT surfaceChanged();
}

QSize WXWaylandSurfaceItem::maximumSize() const
{
    return m_maximumSize;
}

QSize WXWaylandSurfaceItem::minimumSize() const
{
    return m_minimumSize;
}

WXWaylandSurfaceItem::PositionMode WXWaylandSurfaceItem::positionMode() const
{
    return m_positionMode;
}

void WXWaylandSurfaceItem::setPositionMode(PositionMode newPositionMode)
{
    if (m_positionMode == newPositionMode)
        return;
    m_positionMode = newPositionMode;
    Q_EMIT positionModeChanged();
}

void WXWaylandSurfaceItem::move(PositionMode mode)
{
    Q_ASSERT(mode != ManualPosition);

    if (!isVisible())
        return;

    if (mode == PositionFromSurface) {
        setPosition(expectSurfacePosition(mode) - m_positionOffset - QPointF(leftPadding(), topPadding()));
    } else if (mode == PositionToSurface) {
        configureSurface(QRect(expectSurfacePosition(mode), expectSurfaceSize(resizeMode())));
    }
}

QPointF WXWaylandSurfaceItem::positionOffset() const
{
    return m_positionOffset;
}

void WXWaylandSurfaceItem::setPositionOffset(QPointF newPositionOffset)
{
    if (m_positionOffset == newPositionOffset)
        return;
    m_positionOffset = newPositionOffset;
    Q_EMIT positionOffsetChanged();

    updatePosition();
}

bool WXWaylandSurfaceItem::ignoreConfigureRequest() const
{
    return m_ignoreConfigureRequest;
}

void WXWaylandSurfaceItem::setIgnoreConfigureRequest(bool newIgnoreConfigureRequest)
{
    if (m_ignoreConfigureRequest == newIgnoreConfigureRequest)
        return;
    m_ignoreConfigureRequest = newIgnoreConfigureRequest;
    Q_EMIT ignoreConfigureRequestChanged();
}

void WXWaylandSurfaceItem::onSurfaceCommit()
{
    WSurfaceItem::onSurfaceCommit();

    QSize minSize = m_surface->minSize();
    if (!minSize.isValid())
        minSize = QSize(0, 0);

    QSize maxSize = m_surface->maxSize();
    if (maxSize.isValid())
        maxSize = QSize(INT_MAX, INT_MAX);

    if (m_minimumSize != minSize) {
        m_minimumSize = minSize;
        Q_EMIT minimumSizeChanged();
    }

    if (m_maximumSize != maxSize) {
        m_maximumSize = maxSize;
        Q_EMIT maximumSizeChanged();
    }
}

void WXWaylandSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(m_surface);
    connect(m_surface->handle(), &QWXWaylandSurface::beforeDestroy,
            this, &WXWaylandSurfaceItem::releaseResources);
    updatePosition();
}

bool WXWaylandSurfaceItem::resizeSurface(const QSize &newSize)
{
    if (!m_surface->checkNewSize(newSize))
        return false;

    configureSurface(QRect(expectSurfacePosition(m_positionMode), newSize));
    return true;
}

QRectF WXWaylandSurfaceItem::getContentGeometry() const
{
    return m_surface->getContentGeometry();
}

QSizeF WXWaylandSurfaceItem::getContentSize() const
{
    return size() - QSizeF(leftPadding() + rightPadding(), topPadding() + bottomPadding());
}

void WXWaylandSurfaceItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    WSurfaceItem::geometryChange(newGeometry, oldGeometry);

    if (newGeometry.topLeft() != oldGeometry.topLeft()
        && m_positionMode == PositionToSurface && m_surface) {
        configureSurface(QRect(expectSurfacePosition(m_positionMode),
                               expectSurfaceSize(resizeMode())));
    }
}

void WXWaylandSurfaceItem::updatePosition()
{
    if (!m_surface)
        return;

    if (m_positionMode != ManualPosition)
        move(m_positionMode);
}

void WXWaylandSurfaceItem::configureSurface(const QRect &newGeometry)
{
    if (!isVisible())
        return;
    m_surface->configure(newGeometry);
    updateSurfaceState();
}

QPoint WXWaylandSurfaceItem::expectSurfacePosition(PositionMode mode) const
{
    if (mode == PositionFromSurface) {
        const bool useRequestPositon = !m_surface->isBypassManager()
                                       && m_surface->requestConfigureFlags()
                                              .testAnyFlags(WXWaylandSurface::XCB_CONFIG_WINDOW_POSITION);
        return useRequestPositon
                   ? m_surface->requestConfigureGeometry().topLeft()
                   : m_surface->geometry().topLeft();
    } else if (mode == PositionToSurface) {
        return (position() + m_positionOffset + QPointF(leftPadding(), topPadding())).toPoint();
    }

    return m_surface->geometry().topLeft();
}

QSize WXWaylandSurfaceItem::expectSurfaceSize(ResizeMode mode) const
{
    if (mode == SizeFromSurface) {
        const bool useRequestSize = !m_surface->isBypassManager()
                                    && m_surface->requestConfigureFlags()
                                           .testAnyFlags(WXWaylandSurface::XCB_CONFIG_WINDOW_SIZE);
        return useRequestSize
                   ? m_surface->requestConfigureGeometry().size()
                   : m_surface->geometry().size();
    } else if (mode == SizeToSurface) {
        return WXWaylandSurfaceItem::getContentSize().toSize();
    }

    return m_surface->geometry().size();
}

WAYLIB_SERVER_END_NAMESPACE
