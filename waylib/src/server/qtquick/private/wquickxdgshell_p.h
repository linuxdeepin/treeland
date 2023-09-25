// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>
#include <wsurfaceitem.h>

Q_MOC_INCLUDE(<wseat.h>)
Q_MOC_INCLUDE(<wxdgsurface.h>)

#include <QQmlEngine>

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSeat;
class WQuickSurface;
class WXdgSurface;
class WQuickXdgShellPrivate;
class WAYLIB_SERVER_EXPORT WQuickXdgShell : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickXdgShell)
    QML_NAMED_ELEMENT(XdgShell)

public:
    explicit WQuickXdgShell(QObject *parent = nullptr);

Q_SIGNALS:
    void surfaceAdded(WXdgSurface *surface);
    void surfaceRemoved(WXdgSurface *surface);

private:
    void create() override;
    void ownsSocketChange() override;
};

class WAYLIB_SERVER_EXPORT WXdgSurfaceItem : public WSurfaceItem
{
    Q_OBJECT
    Q_PROPERTY(WXdgSurface* surface READ surface WRITE setSurface NOTIFY surfaceChanged)
    Q_PROPERTY(QPointF implicitPosition READ implicitPosition NOTIFY implicitPositionChanged)
    Q_PROPERTY(QSize minimumSize READ minimumSize NOTIFY minimumSizeChanged FINAL)
    Q_PROPERTY(QSize maximumSize READ maximumSize NOTIFY maximumSizeChanged FINAL)
    QML_NAMED_ELEMENT(XdgSurfaceItem)

public:
    explicit WXdgSurfaceItem(QQuickItem *parent = nullptr);
    ~WXdgSurfaceItem();

    WXdgSurface *surface() const;
    void setSurface(WXdgSurface *surface);

    QPointF implicitPosition() const;
    QSize minimumSize() const;
    QSize maximumSize() const;

Q_SIGNALS:
    void surfaceChanged();
    void implicitPositionChanged();
    void minimumSizeChanged();
    void maximumSizeChanged();

private:
    Q_SLOT void onSurfaceCommit() override;
    void initSurface() override;
    bool resizeSurface(const QSize &newSize) override;
    QRectF getContentGeometry() const override;

    void setImplicitPosition(const QPointF &newImplicitPosition);

private:
    QPointer<WXdgSurface> m_surface;
    QPointF m_implicitPosition;
    QSize m_minimumSize;
    QSize m_maximumSize;
};

WAYLIB_SERVER_END_NAMESPACE
