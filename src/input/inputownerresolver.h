// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QPointF>

class QQuickItem;
class QWindow;
class SurfaceWrapper;
class WaylandServer;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSurfaceItem;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

// Decides which surface owns a press at a scene point.
//
// One rule only: a point belongs to the first surface in paint order, top to
// bottom, that declares input there. A client surface declares exactly its
// wl_surface.input_region; a window's compositor-drawn chrome (title bar,
// resize border) declares its own item geometry. A window rectangle declares
// nothing by itself, so a hole in a client's input region passes the press to
// whatever is painted below.
//
// The client side reuses waylib's own hit test (WSurfaceItem::eventItem() asks
// the same input region), so the surface that receives the press and the
// surface that owns it cannot disagree.
class InputOwnerResolver
{
public:
    // The surface that owns @p scenePos, or nullptr when nothing declares it.
    // @p buttons is what the press holds down; a chrome item only declares a
    // press it accepts.
    static SurfaceWrapper *ownerAt(WaylandServer *server,
                                   QWindow *window,
                                   const QPointF &scenePos,
                                   Qt::MouseButtons buttons);

private:
    static QQuickItem *declaringItemAt(QQuickItem *item,
                                       const QPointF &scenePos,
                                       Qt::MouseButtons buttons);
    static bool clientSurfaceDeclares(WSurfaceItem *surfaceItem, const QPointF &scenePos);
    static SurfaceWrapper *surfaceFor(WaylandServer *server, QQuickItem *item);
};
