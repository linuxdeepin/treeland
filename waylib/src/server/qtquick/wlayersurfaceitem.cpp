// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wlayersurfaceitem.h"
#include "wsurfaceitem_p.h"
#include "wlayersurface.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WLayerSurfaceItem::WLayerSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(parent)
{

}

WLayerSurfaceItem::~WLayerSurfaceItem()
{

}

WLayerSurface *WLayerSurfaceItem::layerSurface() const {
    return qobject_cast<WLayerSurface*>(shellSurface());
}

[[maybe_unused]] inline static int32_t getValidSize(int32_t size, int32_t fallback) {
    return size > 0 ? size : fallback;
}

void WLayerSurfaceItem::onSurfaceCommit()
{
    WSurfaceItem::onSurfaceCommit();
}

void WLayerSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(layerSurface());
    connect(layerSurface(), &WWrapObject::aboutToBeInvalidated,
            this, &WLayerSurfaceItem::releaseResources);
}

QRectF WLayerSurfaceItem::getContentGeometry() const
{
   return layerSurface()->getContentGeometry();
}

WAYLIB_SERVER_END_NAMESPACE
