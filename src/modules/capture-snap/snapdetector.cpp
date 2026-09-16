// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "snapdetector.h"

#include <private/qquickitem_p.h>

#include <woutputitem.h>
#include <woutputrenderwindow.h>
#include <wsurface.h>
#include <wsurfaceitem.h>

WAYLIB_SERVER_USE_NAMESPACE

QList<QRectF> SnapDetector::collect(WOutputRenderWindow *renderWindow, WSurface *excludeSurface)
{
    QList<QRectF> result;
    if (!renderWindow || !renderWindow->contentItem())
        return result;

    QList<QPointer<QQuickItem>> outputItems;

    auto items =
        WOutputRenderWindow::paintOrderItemList(renderWindow->contentItem(),
                                                [&outputItems](QQuickItem *item) -> bool {
                                                    if (!item->isVisible())
                                                        return false;
                                                    if (qobject_cast<WOutputItem *>(item)) {
                                                        outputItems.append(item);
                                                        return false;
                                                    }
                                                    if (qobject_cast<WSurfaceItem *>(item))
                                                        return true;
                                                    return false;
                                                });

    for (auto it = items.crbegin(); it != items.crend(); ++it) {
        if (!*it)
            continue;
        auto surfaceItem = qobject_cast<WSurfaceItem *>(*it);
        if (!surfaceItem)
            continue;
        if (excludeSurface && surfaceItem->surface() == excludeSurface)
            continue;
        result.append(surfaceItem->mapRectToScene(surfaceItem->boundingRect()));
    }

    for (const auto &item : std::as_const(outputItems)) {
        if (!item)
            continue;
        result.append(item->mapRectToScene(item->boundingRect()));
    }

    return result;
}

QRectF SnapDetector::hitTest(const QPointF &cursorPos, const QList<QRectF> &snapshot)
{
    for (const auto &rect : snapshot) {
        if (rect.contains(cursorPos))
            return rect;
    }
    return { };
}
