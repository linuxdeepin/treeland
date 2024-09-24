// Copyright (C) 2023-2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wsurfaceitem.h>

#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXWaylandSurface;
class WXWaylandSurfaceItemPrivate;

class WAYLIB_SERVER_EXPORT WXWaylandSurfaceItem : public WSurfaceItem
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WXWaylandSurfaceItem)
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

    inline WXWaylandSurface* xwaylandSurface() const { return qobject_cast<WXWaylandSurface*>(shellSurface()); }
    bool setShellSurface(WToplevelSurface *surface) override;

    WXWaylandSurfaceItem *parentSurfaceItem() const;
    void setParentSurfaceItem(WXWaylandSurfaceItem *newParentSurfaceItem);

    QSize minimumSize() const;
    QSize maximumSize() const;

    PositionMode positionMode() const;
    void setPositionMode(PositionMode newPositionMode);
    Q_INVOKABLE void move(PositionMode mode);

    QPointF positionOffset() const;
    void setPositionOffset(QPointF newPositionOffset);

    bool ignoreConfigureRequest() const;
    void setIgnoreConfigureRequest(bool newIgnoreConfigureRequest);

Q_SIGNALS:
    void parentSurfaceItemChanged();
    void minimumSizeChanged();
    void maximumSizeChanged();
    void positionModeChanged();
    void positionOffsetChanged();
    void ignoreConfigureRequestChanged();

private:
    Q_SLOT void onSurfaceCommit() override;
    void initSurface() override;
    bool doResizeSurface(const QSize &newSize) override;
    QRectF getContentGeometry() const override;
    QSizeF getContentSize() const override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    Q_SLOT void updatePosition();
};

WAYLIB_SERVER_END_NAMESPACE
