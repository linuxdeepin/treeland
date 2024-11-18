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

class WXdgToplevelSurface;
class WXdgToplevelSurfaceItemPrivate;

class WAYLIB_SERVER_EXPORT WXdgToplevelSurfaceItem : public WSurfaceItem
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WXdgToplevelSurfaceItem)
    Q_PROPERTY(QSize minimumSize READ minimumSize NOTIFY minimumSizeChanged FINAL)
    Q_PROPERTY(QSize maximumSize READ maximumSize NOTIFY maximumSizeChanged FINAL)
    QML_NAMED_ELEMENT(XdgToplevelSurfaceItem)

public:
    explicit WXdgToplevelSurfaceItem(QQuickItem *parent = nullptr);
    ~WXdgToplevelSurfaceItem();

    WXdgToplevelSurface *toplevelSurface() const;
    QSize minimumSize() const;
    QSize maximumSize() const;

Q_SIGNALS:
    void minimumSizeChanged();
    void maximumSizeChanged();

private:
    Q_SLOT void onSurfaceCommit() override;
    void initSurface() override;
    QRectF getContentGeometry() const override;
};

WAYLIB_SERVER_END_NAMESPACE
