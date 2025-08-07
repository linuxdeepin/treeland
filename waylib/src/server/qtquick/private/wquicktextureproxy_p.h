// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wquicktextureproxy.h"
#include "private/wglobal_p.h"

#include <QMetaObject>
#include <QMutex>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WQuickTextureProxyPrivate : public WObjectPrivate
{
public:
    WQuickTextureProxyPrivate(WQuickTextureProxy *qq)
        : WObjectPrivate(qq)
    {

    }

    ~WQuickTextureProxyPrivate() override;

    void initSourceItem(QQuickItem *old, QQuickItem *item);
    void updateImplicitSize();

    W_DECLARE_PUBLIC(WQuickTextureProxy)

    QPointer<QQuickItem> sourceItem;
    QRectF sourceRect;
    bool hideSource = false;
    bool mipmap = false;
    QMetaObject::Connection textureChangedConnection;
    QMutex connectionMutex;
};

WAYLIB_SERVER_END_NAMESPACE
