// Copyright (C) 2025 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsessionlocksurfaceitem.h"
#include "wsurfaceitem_p.h"
#include "wsessionlocksurface.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WSessionLockSurfaceItem::WSessionLockSurfaceItem(QQuickItem *parent)
    : WSurfaceItem(parent)
{

}

WSessionLockSurfaceItem::~WSessionLockSurfaceItem()
{

}

WSessionLockSurface *WSessionLockSurfaceItem::sessionLockSurface() const {
    return qobject_cast<WSessionLockSurface*>(shellSurface());
}

void WSessionLockSurfaceItem::onSurfaceCommit()
{
    WSurfaceItem::onSurfaceCommit();
}

void WSessionLockSurfaceItem::initSurface()
{
    WSurfaceItem::initSurface();
    Q_ASSERT(sessionLockSurface());
    connect(sessionLockSurface(), &WWrapObject::aboutToBeInvalidated,
            this, &WSessionLockSurfaceItem::releaseResources);
}

QRectF WSessionLockSurfaceItem::getContentGeometry() const
{
   return sessionLockSurface()->getContentGeometry();
}

WAYLIB_SERVER_END_NAMESPACE
