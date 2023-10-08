// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wquickitemproxy.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WQuickItemProxyPrivate : public WObjectPrivate
{
public:
    WQuickItemProxyPrivate(WQuickItemProxy *qq)
        : WObjectPrivate(qq)
    {

    }

    ~WQuickItemProxyPrivate() override;

    void initSourceItem(QQuickItem *old, QQuickItem *item);
    void onTextureChanged();
    void updateImplicitSize();

    W_DECLARE_PUBLIC(WQuickItemProxy)

    QPointer<QQuickItem> sourceItem;
    QMetaObject::Connection textureChangedConnection;
    QSize textureSize;
    QRectF sourceRect;
    bool hideSource = false;
    bool mipmap = false;
};

WAYLIB_SERVER_END_NAMESPACE
