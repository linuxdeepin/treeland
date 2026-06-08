// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsurfaceitem.h"
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

    void onHasSubsurfaceChanged();
    void updateSubsurfaceItem();
    void onPaddingsChanged();
    void updateContentPosition();
    WSurfaceItem *ensureSubsurfaceItem(WSurface *subsurfaceSurface, QQuickItem *parent);
    void updateSubsurfaceContainers();

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
    QPointer<SubsurfaceContainer> belowSubsurfaceContainer = nullptr;
    QPointer<SubsurfaceContainer> aboveSubsurfaceContainer = nullptr;
    WSurfaceItem::ResizeMode resizeMode = WSurfaceItem::SizeFromSurface;
    WSurfaceItem::Flags surfaceFlags;
    QMarginsF paddings;
    QList<WSurfaceItem*> subsurfaces;
    qreal surfaceSizeRatio = 1.0;
    bool live = true;
    bool subsurfacesVisible = true;

    uint32_t beforeRequestResizeSurfaceStateSeq = 0;
    QRectF boundingRect;
};

WAYLIB_SERVER_END_NAMESPACE

