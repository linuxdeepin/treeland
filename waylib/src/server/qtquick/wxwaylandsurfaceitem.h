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
    QML_NAMED_ELEMENT(XWaylandSurfaceItem)

public:
    explicit WXWaylandSurfaceItem(QQuickItem *parent = nullptr);
    ~WXWaylandSurfaceItem();

    WXWaylandSurface* xwaylandSurface() const;
    bool setShellSurface(WToplevelSurface *surface) override;

    WXWaylandSurfaceItem *parentSurfaceItem() const;
    void setParentSurfaceItem(WXWaylandSurfaceItem *newParentSurfaceItem);

    QSize minimumSize() const;
    QSize maximumSize() const;

    void moveTo(const QPointF &pos, bool configSurface);
    QPointF implicitPosition() const;

Q_SIGNALS:
    void implicitPositionChanged();
    void parentSurfaceItemChanged();
    void minimumSizeChanged();
    void maximumSizeChanged();

private:
    Q_SLOT void onSurfaceCommit() override;
    void initSurface() override;
    bool doResizeSurface(const QSize &newSize) override;
    QRectF getContentGeometry() const override;
    QSizeF getContentSize() const override;
    void surfaceSizeRatioChange() override;
    Q_SLOT void updatePosition();
};

WAYLIB_SERVER_END_NAMESPACE
