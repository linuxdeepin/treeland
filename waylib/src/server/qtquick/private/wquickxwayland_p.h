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
    Q_PROPERTY(WSocket* targetSocket READ targetSocket CONSTANT)
    QML_NAMED_ELEMENT(XWaylandShellV1)

public:
    explicit WXWaylandShellV1(QObject *parent = nullptr);

    QW_NAMESPACE::QWXWaylandShellV1 *shell() const;

Q_SIGNALS:
    void shellChanged();

private:
    WServerInterface *create() override;

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

    Q_INVOKABLE WClient *client() const;
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

    WServerInterface *create() override;
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

WAYLIB_SERVER_END_NAMESPACE
