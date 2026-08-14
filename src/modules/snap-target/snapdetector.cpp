// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "snapdetector.h"

#include <private/qquickitem_p.h>

#include <woutputitem.h>
#include <woutputrenderwindow.h>
#include <wsurface.h>
#include <wsurfaceitem.h>

#include <QSet>

WAYLIB_SERVER_USE_NAMESPACE

QList<SnapTarget> SnapDetector::collect(WOutputRenderWindow *renderWindow,
                                        const QList<WSurface *> &excludeSurfaces)
{
    QList<SnapTarget> result;
    if (!renderWindow || !renderWindow->contentItem())
        return result;

    const QSet<WSurface *> exclude(excludeSurfaces.cbegin(), excludeSurfaces.cend());

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
        if (exclude.contains(surfaceItem->surface()))
            continue;
        SnapTarget target;
        target.rect = surfaceItem->mapRectToScene(surfaceItem->boundingRect());
        target.shellSurface = surfaceItem->shellSurface();
        result.append(target);
    }

    for (const auto &item : std::as_const(outputItems)) {
        if (!item)
            continue;
        SnapTarget target;
        target.rect = item->mapRectToScene(item->boundingRect());
        result.append(target);
    }

    return result;
}

SnapTarget SnapDetector::hitTest(const QPointF &cursorPos, const QList<SnapTarget> &snapshot)
{
    for (const auto &target : snapshot) {
        if (target.rect.contains(cursorPos))
            return target;
    }
    return { };
}
