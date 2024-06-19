// Copyright (C) 2023-2024 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wsurfaceitem.h>

#include <QQmlEngine>

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXdgSurface;
class WXdgSurfaceItemPrivate;

class WAYLIB_SERVER_EXPORT WXdgSurfaceItem : public WSurfaceItem
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WXdgSurfaceItem)
    Q_PROPERTY(QPointF implicitPosition READ implicitPosition NOTIFY implicitPositionChanged)
    Q_PROPERTY(QSize minimumSize READ minimumSize NOTIFY minimumSizeChanged FINAL)
    Q_PROPERTY(QSize maximumSize READ maximumSize NOTIFY maximumSizeChanged FINAL)
    QML_NAMED_ELEMENT(XdgSurfaceItem)

public:
    explicit WXdgSurfaceItem(QQuickItem *parent = nullptr);
    ~WXdgSurfaceItem();

    WXdgSurface* xdgSurface() const;
    QPointF implicitPosition() const;
    QSize minimumSize() const;
    QSize maximumSize() const;

Q_SIGNALS:
    void implicitPositionChanged();
    void minimumSizeChanged();
    void maximumSizeChanged();

private:
    Q_SLOT void onSurfaceCommit() override;
    void initSurface() override;
    QRectF getContentGeometry() const override;
};

WAYLIB_SERVER_END_NAMESPACE
