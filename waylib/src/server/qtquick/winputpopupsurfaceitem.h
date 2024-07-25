// Copyright (C) 2024 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wsurfaceitem.h>
WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WInputPopupSurfaceItem : public WSurfaceItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(InputPopupSurfaceItem)
    Q_PROPERTY(QRect referenceRect READ referenceRect NOTIFY referenceRectChanged)

public:
    explicit WInputPopupSurfaceItem(QQuickItem *parent = nullptr);
    QRect referenceRect() const;

Q_SIGNALS:
    void referenceRectChanged();
};
WAYLIB_SERVER_END_NAMESPACE
