// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>
#include <wsurfaceitem.h>

#include <QQmlEngine>

Q_MOC_INCLUDE(<qwcompositor.h>)
Q_MOC_INCLUDE(<qwxwaylandshellv1.h>)
Q_MOC_INCLUDE("wxwaylandsurface.h")

QW_BEGIN_NAMESPACE
class QWXWayland;
class QWXWaylandSurface;
class QWXWaylandShellV1;
class QWCompositor;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WXWaylandShellV1 : public WQuickWaylandServerInterface
{
    Q_OBJECT
    Q_PROPERTY(QW_NAMESPACE::QWXWaylandShellV1* shell READ shell NOTIFY shellChanged FINAL)
    Q_PROPERTY(WSocket* ownsSocket READ ownsSocket CONSTANT)
    QML_NAMED_ELEMENT(XWaylandShellV1)

public:
    explicit WXWaylandShellV1(QObject *parent = nullptr);

    QW_NAMESPACE::QWXWaylandShellV1 *shell() const;

Q_SIGNALS:
    void shellChanged();

private:
    void create() override;

    QW_NAMESPACE::QWXWaylandShellV1 *m_shell = nullptr;
};

class WXWayland;
class WSeat;
class WXWaylandSurface;
class WAYLIB_SERVER_EXPORT WQuickXWayland : public WQuickWaylandServerInterface
{
    Q_OBJECT
    Q_PROPERTY(bool lazy READ lazy WRITE setLazy NOTIFY lazyChanged FINAL)
    Q_PROPERTY(QW_NAMESPACE::QWCompositor* compositor READ compositor WRITE setCompositor NOTIFY compositorChanged)
    Q_PROPERTY(WSeat* seat READ seat WRITE setSeat NOTIFY seatChanged)
    Q_PROPERTY(QByteArray displayName READ displayName NOTIFY displayNameChanged FINAL)
    QML_NAMED_ELEMENT(XWayland)

public:
    explicit WQuickXWayland(QObject *parent = nullptr);

    bool lazy() const;
    void setLazy(bool newLazy);

    QW_NAMESPACE::QWCompositor *compositor() const;
    void setCompositor(QW_NAMESPACE::QWCompositor *compositor);

    WSeat *seat() const;
    void setSeat(WSeat *newSeat);

    QByteArray displayName() const;

    Q_INVOKABLE wl_client *client() const;
    Q_INVOKABLE pid_t pid() const;

Q_SIGNALS:
    void lazyChanged();
    void compositorChanged();
    void seatChanged();
    void displayNameChanged();
    void ready();
    void surfaceAdded(WXWaylandSurface *surface);
    void surfaceRemoved(WXWaylandSurface *surface);
    void toplevelAdded(WXWaylandSurface *surface);
    void toplevelRemoved(WXWaylandSurface *surface);

private:
    friend class XWayland;

    void create() override;
    void ownsSocketChange() override;
    void tryCreateXWayland();
    void onIsToplevelChanged();
    void addSurface(WXWaylandSurface *surface);
    void removeSurface(WXWaylandSurface *surface);
    void addToplevel(WXWaylandSurface *surface);
    void removeToplevel(WXWaylandSurface *surface);

    WXWayland *xwayland = nullptr;
    QList<WXWaylandSurface*> toplevelSurfaces;
    bool m_lazy = true;
    QW_NAMESPACE::QWCompositor *m_compositor = nullptr;
    WSeat *m_seat = nullptr;
};

class WQuickObserver;
class WAYLIB_SERVER_EXPORT WXWaylandSurfaceItem : public WSurfaceItem
{
    Q_OBJECT
    Q_PROPERTY(WXWaylandSurface* surface READ surface WRITE setSurface NOTIFY surfaceChanged)
    Q_PROPERTY(WXWaylandSurfaceItem* parentSurfaceItem READ parentSurfaceItem WRITE setParentSurfaceItem NOTIFY parentSurfaceItemChanged FINAL)
    Q_PROPERTY(QSize minimumSize READ minimumSize NOTIFY minimumSizeChanged FINAL)
    Q_PROPERTY(QSize maximumSize READ maximumSize NOTIFY maximumSizeChanged FINAL)
    Q_PROPERTY(PositionMode positionMode READ positionMode WRITE setPositionMode NOTIFY positionModeChanged FINAL)
    Q_PROPERTY(QPointF positionOffset READ positionOffset WRITE setPositionOffset NOTIFY positionOffsetChanged FINAL)
    Q_PROPERTY(bool ignoreConfigureRequest READ ignoreConfigureRequest WRITE setIgnoreConfigureRequest NOTIFY ignoreConfigureRequestChanged FINAL)
    QML_NAMED_ELEMENT(XWaylandSurfaceItem)

public:
    enum PositionMode {
        PositionFromSurface,
        PositionToSurface,
        ManualPosition
    };
    Q_ENUM(PositionMode)

    explicit WXWaylandSurfaceItem(QQuickItem *parent = nullptr);
    ~WXWaylandSurfaceItem();

    WXWaylandSurface *surface() const;
    void setSurface(WXWaylandSurface *surface);

    WXWaylandSurfaceItem *parentSurfaceItem() const;
    void setParentSurfaceItem(WXWaylandSurfaceItem *newParentSurfaceItem);

    QSize minimumSize() const;
    QSize maximumSize() const;

    PositionMode positionMode() const;
    void setPositionMode(PositionMode newPositionMode);
    Q_INVOKABLE void move(PositionMode mode);
    bool resizeSurface(const QSize &newSize) override;

    QPointF positionOffset() const;
    void setPositionOffset(QPointF newPositionOffset);

    bool ignoreConfigureRequest() const;
    void setIgnoreConfigureRequest(bool newIgnoreConfigureRequest);

Q_SIGNALS:
    void surfaceChanged();
    void parentSurfaceItemChanged();
    void minimumSizeChanged();
    void maximumSizeChanged();
    void positionModeChanged();
    void positionOffsetChanged();
    void ignoreConfigureRequestChanged();

private:
    Q_SLOT void onSurfaceCommit() override;
    void initSurface() override;
    QRectF getContentGeometry() const override;
    QSizeF getContentSize() const override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

    inline void checkMove(PositionMode mode) {
        if (!m_surface || mode == ManualPosition || !isVisible())
            return;
        doMove(mode);
    }
    void doMove(PositionMode mode);
    Q_SLOT void updatePosition();
    void configureSurface(const QRect &newGeometry);
    // get xwayland surface's position of specified mode
    QPoint expectSurfacePosition(PositionMode mode) const;
    // get xwayland surface's size of specified mode
    QSize expectSurfaceSize(ResizeMode mode) const;

private:
    QPointer<WXWaylandSurface> m_surface;
    QPointer<WXWaylandSurfaceItem> m_parentSurfaceItem;
    QSize m_minimumSize;
    QSize m_maximumSize;
    PositionMode m_positionMode = PositionFromSurface;
    QPointF m_positionOffset;
    bool m_ignoreConfigureRequest = false;
};

WAYLIB_SERVER_END_NAMESPACE
