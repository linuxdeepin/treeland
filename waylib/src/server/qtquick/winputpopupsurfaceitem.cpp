// Copyright (C) 2024 Yixue Wang <wangyixue@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "winputpopupsurfaceitem.h"

#include "winputpopupsurface.h"
WAYLIB_SERVER_BEGIN_NAMESPACE
WInputPopupSurfaceItem::WInputPopupSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(parent)
{
    connect(this, &WInputPopupSurfaceItem::shellSurfaceChanged, this, [this] {
        auto inputPopupSurface = dynamic_cast<WInputPopupSurface*>(shellSurface());
        if (inputPopupSurface) {
            connect(inputPopupSurface, &WInputPopupSurface::cursorRectChanged, this, &WInputPopupSurfaceItem::referenceRectChanged);
        }
    });
}

QRect WInputPopupSurfaceItem::referenceRect() const
{
    auto inputPopupSurface = dynamic_cast<WInputPopupSurface*>(shellSurface());
    if (inputPopupSurface) {
        return inputPopupSurface->cursorRect();
    }
    return QRect();
}

WAYLIB_SERVER_END_NAMESPACE
