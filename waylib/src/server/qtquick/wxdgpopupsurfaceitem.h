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

class WXdgPopupSurface;
class WXdgPopupSurfaceItemPrivate;

class WAYLIB_SERVER_EXPORT WXdgPopupSurfaceItem : public WSurfaceItem
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WXdgPopupSurfaceItem)
    Q_PROPERTY(QPointF implicitPosition READ implicitPosition NOTIFY implicitPositionChanged)
    QML_NAMED_ELEMENT(XdgPopupSurfaceItem)

public:
    explicit WXdgPopupSurfaceItem(QQuickItem *parent = nullptr);
    ~WXdgPopupSurfaceItem();

    WXdgPopupSurface *popupSurface() const;
    QPointF implicitPosition() const;

Q_SIGNALS:
    void implicitPositionChanged();

private:
    Q_SLOT void onSurfaceCommit() override;
    void initSurface() override;
    QRectF getContentGeometry() const override;
};

WAYLIB_SERVER_END_NAMESPACE
