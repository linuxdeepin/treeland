// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "inputownerresolver.h"

#include "core/waylandserver.h"
#include "core/rootsurfacecontainer.h"
#include "surface/surfacewrapper.h"

#include <woutputrenderwindow.h>
#include <wsurfaceitem.h>

#include <private/qquickitem_p.h>

SurfaceWrapper *InputOwnerResolver::ownerAt(WaylandServer *server,
                                            QWindow *window,
                                            const QPointF &scenePos,
                                            Qt::MouseButtons buttons)
{
    auto *renderWindow = qobject_cast<WOutputRenderWindow *>(window);
    if (!renderWindow || !renderWindow->contentItem())
        return nullptr;

    return surfaceFor(server, declaringItemAt(renderWindow->contentItem(), scenePos, buttons));
}

QQuickItem *InputOwnerResolver::declaringItemAt(QQuickItem *item,
                                                const QPointF &scenePos,
                                                Qt::MouseButtons buttons)
{
    if (!item->isVisible() || !item->isEnabled())
        return nullptr;

    // Children are in paint order, bottom to top: walk them backwards so the
    // topmost one decides first. They are also where a surface keeps its own
    // parts: a title bar and the sub-surface items are children of the
    // WSurfaceItem, and paint above its client area.
    //
    // No rectangle pruning here: a press walks the whole visible tree once,
    // which is what keeps attributed and delivered input the same set.
    const auto children = QQuickItemPrivate::get(item)->paintOrderChildItems();
    for (auto child = children.crbegin(); child != children.crend(); ++child) {
        QQuickItem *childItem = *child;
        if (!childItem->isVisible() || !childItem->isEnabled())
            continue;
        const bool inside = childItem->contains(childItem->mapFromScene(scenePos));
        // Only clipping puts a child out of reach. Without it a child may
        // declare a point outside its parent's rectangle: the resize rim of
        // Decoration.qml sits 10px outside the window, on a wrapper that does
        // not contain it.
        if (!inside && childItem->clip())
            continue;
        if (auto *declaring = declaringItemAt(childItem, scenePos, buttons))
            return declaring;
        // Nothing inside declares the point, but Qt would still hand the press
        // to this item: compositor-drawn chrome of a window (title bar, resize
        // border). Containers only hold declarations, they never make one.
        if (qobject_cast<WSurfaceItem *>(childItem) || qobject_cast<SurfaceWrapper *>(childItem))
            continue;
        // shortcut: an item carrying a pointer handler reports every button in
        // acceptedMouseButtons(), so an item that only listens for hover or
        // right clicks also claims a left press here. Upgrade to a per-button
        // registration if such an item ever appears above a window.
        if (inside && (childItem->acceptedMouseButtons() & buttons))
            return childItem;
    }

    // The item itself declares last. A client surface declares exactly its
    // input region; whatever it draws inside itself (borders, shadows) is not
    // ours to attribute. Its EventItem child already declared such a point
    // above (same input region, AllButtons), so this is a backstop, not the
    // common path.
    if (auto *surfaceItem = qobject_cast<WSurfaceItem *>(item)) {
        return clientSurfaceDeclares(surfaceItem, scenePos) ? surfaceItem : nullptr;
    }

    return nullptr;
}

bool InputOwnerResolver::clientSurfaceDeclares(WSurfaceItem *surfaceItem, const QPointF &scenePos)
{
    if (!surfaceItem->effectiveVisible())
        return false;

    // The event item is what a press would be delivered to, and it asks
    // wlr_surface_point_accepts_input() for the point.
    QQuickItem *eventItem = surfaceItem->eventItem();
    return eventItem && eventItem->isVisible()
        && eventItem->contains(eventItem->mapFromScene(scenePos));
}

SurfaceWrapper *InputOwnerResolver::surfaceFor(WaylandServer *server, QQuickItem *item)
{
    auto *rootContainer = server ? server->rootContainer() : nullptr;
    if (!rootContainer)
        return nullptr;

    for (auto *node = item; node; node = node->parentItem()) {
        if (auto *surfaceItem = qobject_cast<WSurfaceItem *>(node)) {
            if (auto *wrapper = rootContainer->getSurface(surfaceItem->surface()))
                return wrapper;
        }
        if (auto *wrapper = qobject_cast<SurfaceWrapper *>(node))
            return wrapper;
    }

    return nullptr;
}
