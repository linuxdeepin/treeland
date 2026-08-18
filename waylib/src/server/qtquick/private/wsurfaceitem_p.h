// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsurfaceitem.h"
#include "wsubsurface.h"
#include "wsurface.h"

#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGRenderNode>
#include <private/qquickitem_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

struct Q_DECL_HIDDEN SurfaceState {
    QRectF contentGeometry;
    QSizeF contentSize;
    qreal bufferScale = 1.0;
};
class SubsurfaceContainer;
class Q_DECL_HIDDEN WSurfaceItemPrivate : public QQuickItemPrivate
{
public:
    WSurfaceItemPrivate();
    ~WSurfaceItemPrivate();

    inline static WSurfaceItemPrivate *get(WSurfaceItem *qq) {
        return qq->d_func();
    }

    void initForSurface();
    void initForDelegate();

    void updateSubsurfaceItem();
    void onPaddingsChanged();
    void updateContentPosition();
    WSurfaceItem *ensureSubsurfaceItem(WSubsurface *subsurface);
    void removeSubsurfaceItem(WSubsurface *subsurface);
    void updateSubsurfaceContainers();
    void updateSubsurfaceContainer(WSubsurface::Place place,
                                   const QList<WSubsurface *> &subsurfaces,
                                   QPointer<SubsurfaceContainer> &container);
    void cleanupSubsurfaceContainers();
    void reorderSubsurfaceItems();
    void updateSubsurfacePositions();

    void resizeSurfaceToItemSize(const QSize &itemSize, const QSize &sizeDiff);
    void updateEventItem(bool forceDestroy);
    void updateEventItemGeometry();
    void doResize(WSurfaceItem::ResizeMode mode);

    inline QSizeF paddingsSize() const {
        return QSizeF(paddings.left() + paddings.right(),
                      paddings.top() + paddings.bottom());
    }

    qreal calculateImplicitWidth() const;
    qreal calculateImplicitHeight() const;
    QRectF calculateBoundingRect() const;
    void updateBoundingRect();

    inline WSurfaceItemContent *getItemContent() const {
        if (delegate || !contentContainer)
            return nullptr;
        auto content = qobject_cast<WSurfaceItemContent*>(contentContainer);
        Q_ASSERT(content);
        return content;
    }

    Q_DECLARE_PUBLIC(WSurfaceItem)
    QPointer<WSurface> surface;
    QPointer<WToplevelSurface> shellSurface;
    std::unique_ptr<SurfaceState> surfaceState;
    QQuickItem *contentContainer = nullptr;
    QQmlComponent *delegate = nullptr;
    bool delegateIsDirty = false;
    QQuickItem *eventItem = nullptr;
    QPointer<SubsurfaceContainer> belowSubsurfaceContainer;
    QPointer<SubsurfaceContainer> aboveSubsurfaceContainer;
    WSurfaceItem::ResizeMode resizeMode = WSurfaceItem::SizeFromSurface;
    WSurfaceItem::Flags surfaceFlags;
    QMarginsF paddings;
    qreal surfaceSizeRatio = 1.0;
    bool live = true;
    bool subsurfacesVisible = true;

    uint32_t beforeRequestResizeSurfaceStateSeq = 0;
    QRectF boundingRect;
    bool ready = false;
};

WAYLIB_SERVER_END_NAMESPACE
