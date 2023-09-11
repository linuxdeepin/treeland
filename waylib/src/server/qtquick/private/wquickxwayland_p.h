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
    Q_PROPERTY(QSize minimumSize READ minimumSize NOTIFY minimumSizeChanged FINAL)
    Q_PROPERTY(QSize maximumSize READ maximumSize NOTIFY maximumSizeChanged FINAL)
    Q_PROPERTY(bool autoConfigurePosition READ autoConfigurePosition WRITE setAutoConfigurePosition NOTIFY autoConfigurePositionChanged FINAL)
    QML_NAMED_ELEMENT(XWaylandSurfaceItem)

public:
    explicit WXWaylandSurfaceItem(QQuickItem *parent = nullptr);
    ~WXWaylandSurfaceItem();

    WXWaylandSurface *surface() const;
    void setSurface(WXWaylandSurface *surface);

    QSize minimumSize() const;
    QSize maximumSize() const;

    bool autoConfigurePosition() const;
    void setAutoConfigurePosition(bool newAutoConfigurePosition);

Q_SIGNALS:
    void surfaceChanged();
    void minimumSizeChanged();
    void maximumSizeChanged();
    void autoConfigurePositionChanged();

private:
    Q_SLOT void onSurfaceCommit() override;
    void initSurface() override;
    bool resizeSurface(const QSize &newSize) override;
    QRectF getContentGeometry() const override;
    QSizeF getContentSize() const override;
    void updateSurfaceGeometry();
    void enableObserver(bool on);

private:
    QPointer<WXWaylandSurface> m_surface;
    QPointer<WQuickObserver> observer;
    QSize m_minimumSize;
    QSize m_maximumSize;
    bool m_autoConfigurePosition = false;
};

WAYLIB_SERVER_END_NAMESPACE
