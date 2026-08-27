// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wpixmanregion.h"

#include <wglobal.h>

#include <QFlags>
#include <QMatrix4x4>
#include <QMargins>

#include <QRect>
#include <QRectF>
#include <QRegion>
#include <QString>
#include <QTransform>
#include <QVarLengthArray>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSGDamageNode;
class WSGDamageTracker;
class WSGViewport;
class WSGDamageTransformNode;
class WSGDamageClipNode;
class WSGDamageGeometryNode;
class WSGDamageBackdropNode;
// Scene-graph node used for GUI damage precomputation.
//
// Tree shape matches QSG: first child is behind later siblings; a node's own
// geometry (if any) is behind its descendants.
//
// Parent owns children. Removing a child does not delete it. Destroying a
// parent deletes remaining children. Not thread-safe.
class WAYLIB_SERVER_EXPORT WSGDamageNode
{
public:
    enum class Type
    {
        Basic = 0,
        Transform,
        Clip,
        Geometry,
    };

    enum DirtyBit
    {
        DirtyMatrix = 1 << 0,
        DirtyGeometry = 1 << 1,
        DirtyContent = 1 << 2,
        DirtyAdded = 1 << 3,
        DirtyStructure = 1 << 4,
        DirtyVisibility = 1 << 5,
        DirtyOpaque = 1 << 6,
        DirtySubtree = 1 << 7,
        DirtySubtreeGeometry = 1 << 8
    };
    Q_DECLARE_FLAGS(DirtyBits, DirtyBit)

    explicit WSGDamageNode(Type type = Type::Basic);
    virtual ~WSGDamageNode();

    WSGDamageNode(const WSGDamageNode &) = delete;
    WSGDamageNode &operator=(const WSGDamageNode &) = delete;

    Type type() const
    {
        return m_type;
    }

    bool hasContent() const
    {
        return m_hasContent;
    }

    void setHasContent(bool hasContent);

    WSGDamageTransformNode *toTransform();
    WSGDamageClipNode *toClip();
    WSGDamageGeometryNode *toGeometry();
    const WSGDamageTransformNode *toTransform() const;
    const WSGDamageClipNode *toClip() const;
    const WSGDamageGeometryNode *toGeometry() const;
    WSGDamageBackdropNode *toBackdrop();
    const WSGDamageBackdropNode *toBackdrop() const;

    void setName(const QString &name)
    {
        m_name = name;
    }

    QString name() const
    {
        return m_name;
    }

    quint64 id() const
    {
        return m_id;
    }

    bool isVisible() const
    {
        return m_visible;
    }

    void setVisible(bool visible);
    void setNeedsBackdrop(bool needsBackdrop);

    bool needsBackdrop() const
    {
        return m_needsBackdrop;
    }

    WSGDamageNode *parent() const
    {
        return m_parent;
    }

    WSGDamageNode *firstChild() const
    {
        return m_firstChild;
    }

    WSGDamageNode *lastChild() const
    {
        return m_lastChild;
    }

    WSGDamageNode *nextSibling() const
    {
        return m_next;
    }

    WSGDamageNode *previousSibling() const
    {
        return m_prev;
    }

    int childCount() const
    {
        return m_childCount;
    }

    void prependChild(WSGDamageNode *child);
    void appendChild(WSGDamageNode *child);
    void insertChildBefore(WSGDamageNode *child, WSGDamageNode *before);
    void insertChildAfter(WSGDamageNode *child, WSGDamageNode *after);
    void removeChild(WSGDamageNode *child);
    void removeAllChildren(); // does not delete

    // After WSGDamageTracker::commit(). World fields are viewport-independent.
    QTransform worldTransform() const
    {
        return m_worldTransform;
    }

    QRect worldBounds() const
    {
        return m_worldBounds;
    }

    QRect subtreeBounds() const
    {
        return m_subtreeAABB;
    }

    const pixman_region32_t *worldOpaqueRegion() const
    {
        return m_worldOpaque.native();
    }

    // Bounds minus front opaque. Backdrop punches this so sampled-behind
    // stays valid for cache; not "on-screen visible". Ancestor clips are
    // already applied to worldBounds, so this stays inside the clip.
    const pixman_region32_t *worldValidRegion() const
    {
        return m_worldValidRegion.native();
    }

    // Bounds minus front needsBackdrop coverage. On-screen draw of this node.
    // Clean nodes only redraw this; dirty nodes also draw the sampled part.
    // Same clip inheritance as worldValidRegion.
    const pixman_region32_t *worldVisibleRegion() const
    {
        return m_worldVisibleRegion.native();
    }

    const pixman_region32_t *behindDamageRegion() const
    {
        return m_behindDamage.native();
    }

    // Dirty bits from setters, not "content dirty for this viewport frame".
    DirtyBits dirty() const
    {
        return m_dirty;
    }

    bool isDirty() const
    {
        return m_dirty != 0;
    }

protected:
    void markDirty(DirtyBits bits);
    bool m_hasContent = false;
    friend class WSGDamageTracker;
    friend class WSGDamageNodeTestAccess;
    friend class WSGDamageClipNode;
    friend class WSGDamageGeometryNode;
    void attach(WSGDamageNode *child, WSGDamageNode *prev, WSGDamageNode *next);
    void unlink(WSGDamageNode *child);
    void adopt(WSGDamageNode *child);
    // clipOuter is the world AABB of ancestor clips (damage/bounds; ignores
    // radius). clipInner is the world opaque mask: rectangular clips use the
    // inner AABB, rounded clips use the AABB minus corner squares. Null means
    // unbounded. Empty (non-null) means nothing is visible / opaque.
    void updateWorld(const QTransform &parentWorld,
                     bool parentWorldChanged,
                     WPixmanRegion &worldDamage,
                     WPixmanRegion &backdropDamage,
                     const QRect *clipOuter = nullptr,
                     const WPixmanRegion *clipInner = nullptr,
                     QVarLengthArray<WSGDamageNode *, 64> *ownDamageNodes = nullptr);
    void deactivateWorld(WPixmanRegion &worldDamage,
                         QVarLengthArray<WSGDamageNode *, 64> *ownDamageNodes);
    void clearBehindDamageRecursive();
    void computeWorldVisibility(WPixmanRegion &worldFrontOpaque, WPixmanRegion &worldFrontBackdrop);
    void computeWorldVisibilityImpl(WPixmanRegion &worldFrontOpaque,
                                    WPixmanRegion &worldFrontBackdrop);
    void resetWorldVisibleRecursive();
    void invalidateCommittedStateRecursive();
    void commitState();
    Type m_type;
    quint64 m_id = 0;
    QString m_name;

    WSGDamageNode *m_parent = nullptr;
    WSGDamageNode *m_firstChild = nullptr;
    WSGDamageNode *m_lastChild = nullptr;
    WSGDamageNode *m_prev = nullptr;
    WSGDamageNode *m_next = nullptr;
    int m_childCount = 0;
    int m_subtreeBackdropCount = 0;

    bool m_visible = true;
    bool m_needsBackdrop = false;

    DirtyBits m_dirty = DirtyAdded;
    DirtyBits m_deferredDirty;

    QTransform m_worldTransform;
    QRect m_worldBounds;
    QRect m_subtreeAABB;
    WPixmanRegion m_worldOpaque;
    WPixmanRegion m_subtreeWorldOpaque;
    WPixmanRegion m_ownDamage;
    WPixmanRegion m_behindDamage;
    WPixmanRegion m_pendingRemovedDamage;
    WPixmanRegion m_worldValidRegion;
    WPixmanRegion m_worldFrontOpaque;
    WPixmanRegion m_worldVisibleRegion;
    WPixmanRegion m_worldFrontBackdrop;
    QRect m_committedWorldBounds;
    QRect m_committedSubtreeAABB;
    bool m_committedVisible = false;
    bool m_effectiveVisible = false;
    bool m_subtreeVisibilityEmpty = true;
    bool m_deferDirtyWhileInactive = false;
    bool m_commitPending = false;

    static quint64 s_nextId;
};

class WAYLIB_SERVER_EXPORT WSGDamageTransformNode : public WSGDamageNode
{
public:
    WSGDamageTransformNode();

    void setMatrix(const QTransform &matrix);
    void setMatrix(const QMatrix4x4 &matrix);

    QTransform matrix() const
    {
        return m_matrix;
    }

    void setTranslation(qreal x, qreal y);
    void setScale(qreal sx, qreal sy);
    void setRotation(qreal degrees, Qt::Axis axis = Qt::ZAxis);

private:
    friend class WSGDamageNode;
    friend class WSGDamageTracker;
    QTransform m_matrix;
};

class WAYLIB_SERVER_EXPORT WSGDamageClipNode : public WSGDamageNode
{
public:
    WSGDamageClipNode();

    // Local clip rectangle. Same meaning as QSGClipNode::clipRect(): the
    // scissor when rectangular, otherwise the AABB of the stencil geometry.
    // Damage/bounds use this AABB even when radius() > 0.
    void setClipRect(const QRectF &rect);

    QRectF clipRect() const
    {
        return m_clipRect;
    }

    // Same as QSGClipNode::isRectangular(). Unknown stencil clips
    // (!rectangular && radius == 0) contribute no descendant opaque.
    void setIsRectangular(bool rectangular);

    bool isRectangular() const
    {
        return m_rectangular;
    }

    // Corner radius from QQuickDefaultClipNode. Outer clip ignores it;
    // opaque is the AABB minus ceil(radius) corner squares.
    void setRadius(qreal radius);

    qreal radius() const
    {
        return m_radius;
    }

private:
    friend class WSGDamageNode;
    friend class WSGDamageTracker;
    QRectF m_clipRect;
    qreal m_radius = 0;
    bool m_rectangular = true;
};

class WAYLIB_SERVER_EXPORT WSGDamageGeometryNode : public WSGDamageNode
{
public:
    WSGDamageGeometryNode();

    void setBoundingRect(const QRectF &rect);

    QRectF boundingRect() const
    {
        return m_boundingRect;
    }

    // Content-local opaque pixels. Origin is boundingRect top-left.
    void setOpaqueRegion(const pixman_region32_t *localOpaque);

    void setOpaqueRegion(const WPixmanRegion &localOpaque)
    {
        setOpaqueRegion(localOpaque.native());
    }

    const pixman_region32_t *opaqueRegion() const
    {
        return m_opaqueRegion.native();
    }

    // Marks every pixel inside the (inner-aligned) content box as opaque.
    // Recomputed when the bounding rect changes.
    void setFullyOpaque(bool fullyOpaque);

    bool isFullyOpaque() const
    {
        return m_fullyOpaque;
    }

    // Content-local dirty pixels. Origin is boundingRect top-left.
    // Clipped to the content box on commit.
    void setOpaqueRegion(const QRegion &localOpaque);
    void markContentDirty(const pixman_region32_t *localRegion);
    void markContentDirty(const QRect &localRect);
    void markContentDirty(const QRegion &localRegion);

protected:
    explicit WSGDamageGeometryNode(Type type);

private:
    friend class WSGDamageNode;
    friend class WSGDamageTracker;
    void syncFullyOpaqueRegion();

    QRectF m_boundingRect;
    WPixmanRegion m_opaqueRegion;
    WPixmanRegion m_pendingContentDamage;
    bool m_fullyOpaque = false;
};

// Damage node for glass/blur content backed by a WRenderBufferNode cache.
// Viewport-independent bookkeeping of the node's own cache texture: the
// tracker accumulates the frame damage that invalidates the cached backdrop
// into pendingRecopy() (scene space); the renderer subtracts every region it
// actually recaptured, so unconsumed damage persists to later frames.
class WAYLIB_SERVER_EXPORT WSGDamageBackdropNode : public WSGDamageGeometryNode
{
public:
    WSGDamageBackdropNode();

    // Scene-space regions of the backdrop cache that are stale.
    const WPixmanRegion &pendingRecopy() const
    {
        return m_pendingRecopy;
    }

    void accumulateRecopy(const pixman_region32_t *damage);

    // Renderer-side: drop the regions that were recaptured into the cache.
    void consumeRecopy(const QRegion &applied);

    // The cache samples beyond the drawn bounds by this margin (blur radius
    // etc.). Mirrors WRenderBufferNode::damageExpansion().
    void setRecopyExpansion(const QMargins &expansion)
    {
        m_recopyExpansion = expansion;
    }

    const QMargins &recopyExpansion() const
    {
        return m_recopyExpansion;
    }

private:
    friend class WSGDamageTracker;
    friend class WSGDamageNodeTestAccess;
    WPixmanRegion m_pendingRecopy;
    QMargins m_recopyExpansion;
};

class WSGDamageNodeTestAccess
{
public:
    static const pixman_region32_t *ownDamage(const WSGDamageNode *n)
    {
        return n->m_ownDamage.native();
    }

    static const WPixmanRegion *pendingRecopy(const WSGDamageNode *n)
    {
        if (const auto *backdrop = n->toBackdrop())
            return &backdrop->m_pendingRecopy;
        return nullptr;
    }
};

WAYLIB_SERVER_END_NAMESPACE

Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WSGDamageNode::DirtyBits)
