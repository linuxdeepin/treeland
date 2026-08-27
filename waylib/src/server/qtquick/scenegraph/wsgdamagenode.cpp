// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgdamagenode_p.h"

#include <QMatrix4x4>
#include <QVector3D>
#include <QVarLengthArray>
#include <QtCore/qglobal.h>
#include <QtNumeric>
WAYLIB_SERVER_BEGIN_NAMESPACE

quint64 WSGDamageNode::s_nextId = 1;

static QMatrix4x4 contentToWorld(const QMatrix4x4 &world, const QRectF &bounds)
{
    if (Q_LIKELY(qFuzzyIsNull(bounds.x()) && qFuzzyIsNull(bounds.y())))
        return world;
    QMatrix4x4 offset;
    offset.translate(float(bounds.x()), float(bounds.y()));
    // Column-vector convention: world * offset applies the content offset
    // first (content-local -> item-local), then the world matrix. The old
    // QTransform row-vector code wrote this as offset * world.
    return world * offset;
}

// True when matrix is a pure 2D affine map, i.e. exactly what a QTransform
// can represent. Only then is QTransform-based (inner) mapping faithful.
static bool is2DAffine(const QMatrix4x4 &matrix)
{
    const float *m = matrix.constData();
    return qFuzzyIsNull(m[2]) && qFuzzyIsNull(m[6])
        && qFuzzyIsNull(m[8]) && qFuzzyIsNull(m[9])
        && qFuzzyCompare(m[10], 1.0f) && qFuzzyIsNull(m[11])
        && qFuzzyIsNull(m[3]) && qFuzzyIsNull(m[7])
        && qFuzzyIsNull(m[14]) && qFuzzyCompare(m[15], 1.0f);
}


// Inner mapping through a world matrix. Only 2D axis-aligned transforms can
// guarantee pixel coverage; 3D or sheared transforms conservatively cover
// nothing (damage still uses the outer bound above).
static QRect mapInner(const QMatrix4x4 &matrix, const QRectF &local)
{
    if (Q_UNLIKELY(!local.isValid() || local.width() <= 0.0 || local.height() <= 0.0))
        return { };
    if (!is2DAffine(matrix))
        return { };
    return mapInner(matrix.toTransform(), local);
}

static WPixmanRegion mapRegionOuter(const QMatrix4x4 &matrix, const WPixmanRegion &region)
{
    if (region.isEmpty())
        return { };
    if (matrix.isIdentity())
        return region;
    if (is2DAffine(matrix))
        return region.mappedOuter(matrix.toTransform());
    int count = 0;
    const pixman_box32_t *boxes = region.rectangles(&count);
    WPixmanRegion mapped;
    for (int i = 0; i < count; ++i) {
        mapped += mapOuter(matrix, QRectF(boxes[i].x1, boxes[i].y1,
                                          boxes[i].x2 - boxes[i].x1, boxes[i].y2 - boxes[i].y1));
    }
    return mapped;
}

static WPixmanRegion mapRegionInner(const QMatrix4x4 &matrix, const WPixmanRegion &region)
{
    if (region.isEmpty())
        return { };
    if (matrix.isIdentity())
        return region;
    if (!is2DAffine(matrix))
        return { };
    return region.mappedInner(matrix.toTransform());
}

static QRect expandedSampleBounds(const WSGDamageBackdropNode *backdrop)
{
    const QRectF local = backdrop->boundingRect().marginsAdded(backdrop->recopyExpansion());
    return mapOuter(backdrop->worldTransform().toTransform(), local);
}

static WPixmanRegion expandedBackdropDamage(const WSGDamageBackdropNode *backdrop,
                                            const WPixmanRegion &source,
                                            const QRect &sample)
{
    const QMargins &expansion = backdrop->recopyExpansion();
    if (expansion.isNull())
        return source;

    // Expansion is item-local, not scene pixels. Invert the projected local
    // plane rather than the 4D matrix: this also handles nested 3D transforms.
    const QTransform world = backdrop->worldTransform().toTransform();
    bool invertible = false;
    const QTransform local = world.inverted(&invertible);
    if (!invertible)
        return WPixmanRegion(sample);
    WPixmanRegion expanded = dilateRegion(source.mappedOuter(local), expansion).mappedOuter(world);
    expanded &= sample;
    return expanded;
}

WSGDamageNode::WSGDamageNode()
    : WSGDamageNode(Type::Basic)
{
}

WSGDamageNode::WSGDamageNode(Type type)
    : m_type(type)
    , m_id(s_nextId++)
{
}

WSGDamageNode::~WSGDamageNode()
{
    if (m_parent)
        m_parent->removeChild(this);

    while (m_firstChild) {
        WSGDamageNode *child = m_firstChild;
        unlink(child);
        delete child;
    }
}

WSGDamageTransformNode *WSGDamageNode::toTransform()
{
    return m_type == Type::Transform ? static_cast<WSGDamageTransformNode*>(this) : nullptr;
}

WSGDamageClipNode *WSGDamageNode::toClip()
{
    return m_type == Type::Clip ? static_cast<WSGDamageClipNode*>(this) : nullptr;
}

WSGDamageGeometryNode *WSGDamageNode::toGeometry()
{
    return m_type == Type::Geometry ? static_cast<WSGDamageGeometryNode*>(this) : nullptr;
}

const WSGDamageTransformNode *WSGDamageNode::toTransform() const
{
    return m_type == Type::Transform ? static_cast<const WSGDamageTransformNode*>(this) : nullptr;
}

const WSGDamageClipNode *WSGDamageNode::toClip() const
{
    return m_type == Type::Clip ? static_cast<const WSGDamageClipNode*>(this) : nullptr;
}

const WSGDamageGeometryNode *WSGDamageNode::toGeometry() const
{
    return m_type == Type::Geometry ? static_cast<const WSGDamageGeometryNode*>(this) : nullptr;
}

WSGDamageBackdropNode *WSGDamageNode::toBackdrop()
{
    return m_type == Type::Geometry ? dynamic_cast<WSGDamageBackdropNode*>(this) : nullptr;
}

const WSGDamageBackdropNode *WSGDamageNode::toBackdrop() const
{
    return m_type == Type::Geometry ? dynamic_cast<const WSGDamageBackdropNode*>(this) : nullptr;
}

void WSGDamageNode::setVisible(bool visible)
{
    if (m_visible == visible)
        return;
    m_visible = visible;
    markDirty(DirtyVisibility);
}

void WSGDamageNode::setNeedsBackdrop(bool needsBackdrop)
{
    if (m_needsBackdrop == needsBackdrop)
        return;
    const int delta = needsBackdrop ? 1 : -1;
    for (WSGDamageNode *node = this; node; node = node->m_parent)
        node->m_subtreeBackdropCount += delta;
    m_needsBackdrop = needsBackdrop;
    markDirty(DirtyOpaque);
}

void WSGDamageNode::setHasContent(bool hasContent)
{
    if (m_type != Type::Geometry) {
        Q_ASSERT_X(!hasContent,
                   "WSGDamageNode::setHasContent",
                   "only geometry nodes can have content");
        return;
    }
    if (m_hasContent == hasContent)
        return;
    if (!hasContent && !m_committedWorldBounds.isEmpty())
        m_pendingRemovedDamage += m_committedWorldBounds;
    m_hasContent = hasContent;
    markDirty(DirtyGeometry);
}

void WSGDamageNode::markDirty(DirtyBits bits)
{
    constexpr DirtyBits geometryBits = DirtyMatrix | DirtyGeometry | DirtyAdded | DirtyStructure
        | DirtyVisibility | DirtyOpaque | DirtySubtreeGeometry;
    const bool geometryDirty = bits & geometryBits;

    m_dirty |= bits;
    for (WSGDamageNode *p = m_parent; p; p = p->m_parent) {
        const bool alreadyDirty = p->m_dirty & DirtySubtree;
        const bool alreadyGeometryDirty = !geometryDirty || (p->m_dirty & DirtySubtreeGeometry);
        p->m_dirty |= DirtySubtree;
        if (geometryDirty)
            p->m_dirty |= DirtySubtreeGeometry;
        if (alreadyDirty && alreadyGeometryDirty)
            break;
    }
}

void WSGDamageNode::adopt(WSGDamageNode *child)
{
    if (child->m_parent)
        child->m_parent->removeChild(child);
}

void WSGDamageNode::attach(WSGDamageNode *child, WSGDamageNode *prev, WSGDamageNode *next)
{
    Q_ASSERT(!child->m_parent);
    for (WSGDamageNode *ancestor = this; ancestor; ancestor = ancestor->m_parent)
        Q_ASSERT(ancestor != child);
    child->m_parent = this;
    child->m_prev = prev;
    child->m_next = next;
    if (prev)
        prev->m_next = child;
    else
        m_firstChild = child;
    if (next)
        next->m_prev = child;
    else
        m_lastChild = child;
    ++m_childCount;
    for (WSGDamageNode *p = this; p; p = p->m_parent) {
        p->m_subtreeBackdropCount += child->m_subtreeBackdropCount;
        if (!child->m_subtreeVisibilityEmpty)
            p->m_subtreeVisibilityEmpty = false;
    }
    child->markDirty(DirtyAdded);
    markDirty(DirtyStructure);
}

void WSGDamageNode::unlink(WSGDamageNode *child)
{
    if (child->m_prev)
        child->m_prev->m_next = child->m_next;
    else
        m_firstChild = child->m_next;
    if (child->m_next)
        child->m_next->m_prev = child->m_prev;
    else
        m_lastChild = child->m_prev;
    child->m_parent = nullptr;
    child->m_prev = nullptr;
    child->m_next = nullptr;
    --m_childCount;
    for (WSGDamageNode *p = this; p; p = p->m_parent) {
        p->m_subtreeBackdropCount -= child->m_subtreeBackdropCount;
    }
}

void WSGDamageNode::prependChild(WSGDamageNode *child)
{
    Q_ASSERT(child);
    Q_ASSERT(child != this);
    adopt(child);
    attach(child, nullptr, m_firstChild);
}

void WSGDamageNode::appendChild(WSGDamageNode *child)
{
    Q_ASSERT(child);
    Q_ASSERT(child != this);
    adopt(child);
    attach(child, m_lastChild, nullptr);
}

void WSGDamageNode::insertChildBefore(WSGDamageNode *child, WSGDamageNode *before)
{
    Q_ASSERT(child);
    Q_ASSERT(child != this);
    if (!before) {
        appendChild(child);
        return;
    }
    Q_ASSERT(before->m_parent == this);
    adopt(child);
    attach(child, before->m_prev, before);
}

void WSGDamageNode::insertChildAfter(WSGDamageNode *child, WSGDamageNode *after)
{
    Q_ASSERT(child);
    Q_ASSERT(child != this);
    if (!after) {
        prependChild(child);
        return;
    }
    Q_ASSERT(after->m_parent == this);
    adopt(child);
    attach(child, after, after->m_next);
}

void WSGDamageNode::removeChild(WSGDamageNode *child)
{
    Q_ASSERT(child);
    Q_ASSERT(child->m_parent == this);
    m_pendingRemovedDamage += child->m_committedSubtreeAABB;
    unlink(child);
    markDirty(DirtyStructure);
}

void WSGDamageNode::removeAllChildren()
{
    while (m_firstChild)
        removeChild(m_firstChild);
}

void WSGDamageNode::updateWorld(const QMatrix4x4 &parentWorld,
                                bool parentWorldChanged,
                                WPixmanRegion &worldDamage,
                                WPixmanRegion &backdropDamage,
                                const QRect *clipOuter,
                                const WPixmanRegion *clipInner)
{
    m_commitPending = true;
    m_dirty |= m_deferredDirty;
    m_deferredDirty = { };
    m_deferDirtyWhileInactive = false;
    m_ownDamage = m_pendingRemovedDamage;
    m_pendingRemovedDamage = { };

    constexpr DirtyBits localGeometryBits =
        DirtyMatrix | DirtyGeometry | DirtyAdded | DirtyVisibility | DirtyOpaque;
    const bool matrixChanged =
        parentWorldChanged || (m_type == Type::Transform && (m_dirty & DirtyMatrix));
    const bool localGeometryDirty = matrixChanged || (m_dirty & localGeometryBits);
    const bool subtreeGeometryDirty =
        localGeometryDirty || (m_dirty & (DirtyStructure | DirtySubtreeGeometry));
    const bool contentDirty = m_dirty & DirtyContent;

    if (localGeometryDirty) {
        if (m_type == Type::Transform)
            m_worldTransform = parentWorld * static_cast<WSGDamageTransformNode*>(this)->m_matrix;
        else
            m_worldTransform = parentWorld;
    }

    // Once a clipped or hidden node has committed as inactive, content-only
    // changes cannot make it visible. Defer them until visibility or geometry
    // changes invalidate the cached inactive state.
    if (Q_UNLIKELY(!m_visible || (clipOuter && !m_committedVisible && !subtreeGeometryDirty))) {
        deactivateWorld(worldDamage);
        return;
    }

    const QRect *childClipOuter = clipOuter;
    const WPixmanRegion *childClipInner = clipInner;
    QRect clipOuterStorage;
    WPixmanRegion clipInnerStorage;
    bool clipChanged = false;
    if (m_type == Type::Clip) {
        auto *clip = static_cast<WSGDamageClipNode*>(this);
        clipOuterStorage = mapOuter(m_worldTransform, clip->m_clipRect);
        if (clipOuter)
            clipOuterStorage = clipOuterStorage.intersected(*clipOuter);
        if (clip->m_radius > 0) {
            clipInnerStorage = mapRegionInner(m_worldTransform,
                                              roundedRectInnerRegion(clip->m_clipRect, clip->m_radius));
        } else if (clip->m_rectangular) {
            clipInnerStorage = WPixmanRegion(mapInner(m_worldTransform, clip->m_clipRect));
        } else {
            clipInnerStorage = { };
        }
        if (clipInner)
            clipInnerStorage &= *clipInner;
        else if (clipOuter)
            clipInnerStorage &= *clipOuter;
        childClipOuter = &clipOuterStorage;
        childClipInner = &clipInnerStorage;
        clipChanged = matrixChanged || (m_dirty & (DirtyGeometry | DirtyAdded | DirtyVisibility));
    }

    if (Q_UNLIKELY(childClipOuter && childClipOuter->isEmpty() && !hasContent())) {
        deactivateWorld(worldDamage);
        return;
    }

    m_effectiveVisible = true;
    const bool appearing = !m_committedVisible || (m_dirty & DirtyAdded);
    if (hasContent()) {
        auto *geo = static_cast<WSGDamageGeometryNode*>(this);
        QMatrix4x4 c2w;
        if (localGeometryDirty) {
            if (geo->m_fullyOpaque)
                geo->syncFullyOpaqueRegion();

            m_worldBounds = mapOuter(m_worldTransform, geo->m_boundingRect);
            if (clipOuter)
                m_worldBounds = m_worldBounds.intersected(*clipOuter);
            c2w = contentToWorld(m_worldTransform, geo->m_boundingRect);
            m_worldOpaque = mapRegionInner(c2w, geo->m_opaqueRegion);
            if (clipInner)
                m_worldOpaque &= *clipInner;
            else if (clipOuter)
                m_worldOpaque &= *clipOuter;

            if (appearing) {
                m_ownDamage += m_worldBounds;
            } else {
                const bool visibilityFlipped = m_effectiveVisible != m_committedVisible;
                const bool shapeChanged =
                    matrixChanged || (m_dirty & (DirtyGeometry | DirtyOpaque)) || visibilityFlipped;
                if (shapeChanged) {
                    m_ownDamage += m_committedWorldBounds;
                    m_ownDamage += m_worldBounds;
                }
            }
        }
        if (!appearing && contentDirty) {
            if (!localGeometryDirty)
                c2w = contentToWorld(m_worldTransform, geo->m_boundingRect);
            WPixmanRegion mapped = mapRegionOuter(c2w, geo->m_pendingContentDamage);
            mapped &= m_worldBounds;
            m_ownDamage += mapped;
        }
        if (localGeometryDirty || contentDirty)
            geo->m_pendingContentDamage = { };
    } else if (localGeometryDirty) {
        m_worldBounds = { };
        m_worldOpaque = { };
        if (auto *geo = toGeometry())
            geo->m_pendingContentDamage = { };
    }

    m_behindDamage = worldDamage;
    WPixmanRegion extra;
    if (m_needsBackdrop && hasContent()) {
        if (auto *backdrop = toBackdrop()) {
            QRect sample = expandedSampleBounds(backdrop);
            if (sample.isEmpty())
                sample = m_worldBounds;
            WPixmanRegion source(sample);
            source &= m_behindDamage;
            // Newly sampled pixels need recapture even when the background
            // itself did not change. Pending debt survives until consumed by
            // each output, independently of this frame's damage.
            const QRect bounds = m_worldBounds;
            if (appearing || (!bounds.isEmpty() && bounds != m_committedWorldBounds)) {
                WPixmanRegion fresh(sample);
                if (!appearing && !m_committedWorldBounds.isEmpty())
                    fresh -= m_committedWorldBounds;
                source += fresh;
            }
            if (!source.isEmpty()) {
                backdrop->accumulateRecopy(source.native());
                extra = expandedBackdropDamage(backdrop, source, sample);
                backdropDamage += source;
                backdropDamage += extra;
            }
        } else if (!m_worldBounds.isEmpty()) {
            extra = m_behindDamage & m_worldBounds;
            backdropDamage += extra;
        }
    }

    if (hasContent() && !m_worldOpaque.isEmpty())
        worldDamage -= m_worldOpaque;
    // A node's own pixels and grouping-node removal holes are behind its
    // children, so a backdrop child samples them before later siblings.
    worldDamage += m_ownDamage;
    worldDamage += extra;

    QRect subtree = subtreeGeometryDirty && hasContent() ? m_worldBounds : QRect();
    const bool rebuildOpaqueCache = subtreeGeometryDirty && m_parent && m_subtreeBackdropCount == 0;
    WPixmanRegion subtreeOpaque;
    if (rebuildOpaqueCache && hasContent())
        subtreeOpaque += m_worldOpaque;

    const bool childWorldChanged = matrixChanged || appearing || clipChanged;
    for (WSGDamageNode *child = m_firstChild; child; child = child->m_next) {
        const bool skipChild = !childWorldChanged && !child->isDirty()
            && (!child->m_effectiveVisible || child->m_subtreeBackdropCount == 0);
        if (skipChild) {
            child->m_ownDamage = { };
            child->m_behindDamage = worldDamage;
            if (child->m_effectiveVisible)
                worldDamage -= child->m_subtreeWorldOpaque;
        } else {
            child->updateWorld(m_worldTransform,
                               childWorldChanged,
                               worldDamage,
                               backdropDamage,
                               childClipOuter,
                               childClipInner);
        }
        if (subtreeGeometryDirty)
            subtree = subtree.united(child->m_subtreeAABB);
        if (rebuildOpaqueCache)
            subtreeOpaque += child->m_subtreeWorldOpaque;
    }
    // Removed descendants can have been in front of an opaque remaining
    // child. Restore their hole after processing this group's children.
    if (!hasContent())
        worldDamage += m_ownDamage;
    if (subtreeGeometryDirty) {
        if (childClipOuter)
            subtree = subtree.intersected(*childClipOuter);
        m_subtreeAABB = subtree;
        if (rebuildOpaqueCache)
            m_subtreeWorldOpaque = subtreeOpaque;
        else
            m_subtreeWorldOpaque = { };
    }

    // A non-empty clip can still exclude an entire subtree. Cache that fact
    // so later content changes stop at this node until geometry or clip state
    // changes.
    if (clipOuter && m_subtreeAABB.isEmpty()) {
        m_effectiveVisible = false;
        m_subtreeWorldOpaque = { };
    }
}

void WSGDamageNode::deactivateWorld(WPixmanRegion &worldDamage)
{
    m_effectiveVisible = false;
    m_deferDirtyWhileInactive = true;
    if (m_committedVisible)
        m_ownDamage += m_committedSubtreeAABB;
    m_worldBounds = { };
    m_subtreeAABB = { };
    m_worldOpaque = { };
    m_subtreeWorldOpaque = { };
    m_behindDamage = worldDamage;
    worldDamage += m_ownDamage;
}

void WSGDamageNode::clearBehindDamageRecursive()
{
    m_behindDamage = { };
    for (WSGDamageNode *child = m_firstChild; child; child = child->m_next)
        child->clearBehindDamageRecursive();
}

void WSGDamageNode::resetWorldVisibleRecursive()
{
    if (m_subtreeVisibilityEmpty)
        return;

    m_worldValidRegion = { };
    m_worldFrontOpaque = { };
    m_worldVisibleRegion = { };
    m_worldFrontBackdrop = { };
    for (WSGDamageNode *child = m_firstChild; child; child = child->m_next)
        child->resetWorldVisibleRecursive();
    m_subtreeVisibilityEmpty = true;
}

void WSGDamageNode::computeWorldVisibility(WPixmanRegion &worldFrontOpaque,
                                           WPixmanRegion &worldFrontBackdrop)
{
    if (Q_UNLIKELY(!m_effectiveVisible)) {
        resetWorldVisibleRecursive();
        commitState();
        return;
    }

    constexpr int kLocalizeRegionRectThreshold = 16;
    const bool localize = m_parent && m_firstChild && !m_subtreeAABB.isEmpty()
        && worldFrontOpaque.rectCount() + worldFrontBackdrop.rectCount()
            > kLocalizeRegionRectThreshold;
    if (!localize) {
        computeWorldVisibilityImpl(worldFrontOpaque, worldFrontBackdrop);
        return;
    }

    // A subtree can only change front coverage inside its AABB. Restricting
    // the working regions prevents every descendant from scanning unrelated
    // rectangles accumulated by spatially distant front siblings.
    WPixmanRegion localFrontOpaque;
    localFrontOpaque.setIntersection(worldFrontOpaque.native(), m_subtreeAABB);
    WPixmanRegion localFrontBackdrop;
    localFrontBackdrop.setIntersection(worldFrontBackdrop.native(), m_subtreeAABB);
    computeWorldVisibilityImpl(localFrontOpaque, localFrontBackdrop);

    worldFrontOpaque -= m_subtreeAABB;
    worldFrontOpaque += localFrontOpaque;
    worldFrontBackdrop -= m_subtreeAABB;
    worldFrontBackdrop += localFrontBackdrop;
}

void WSGDamageNode::computeWorldVisibilityImpl(WPixmanRegion &worldFrontOpaque,
                                               WPixmanRegion &worldFrontBackdrop)
{
    m_subtreeVisibilityEmpty = false;

    for (WSGDamageNode *child = m_lastChild; child; child = child->m_prev)
        child->computeWorldVisibility(worldFrontOpaque, worldFrontBackdrop);

    if (!hasContent()) {
        m_worldFrontOpaque = { };
        m_worldValidRegion = { };
        m_worldFrontBackdrop = { };
        m_worldVisibleRegion = { };
        commitState();
        return;
    }

    m_worldFrontOpaque.setIntersection(worldFrontOpaque.native(), m_worldBounds);
    m_worldValidRegion = WPixmanRegion(m_worldBounds) - m_worldFrontOpaque;
    m_worldFrontBackdrop.setIntersection(worldFrontBackdrop.native(), m_worldBounds);
    m_worldVisibleRegion = WPixmanRegion(m_worldBounds) - m_worldFrontBackdrop;

    if (m_needsBackdrop) {
        if (!m_worldBounds.isEmpty()) {
            worldFrontOpaque -= m_worldBounds;
            worldFrontBackdrop += m_worldBounds;
        }
        commitState();
        return;
    }

    if (Q_UNLIKELY(!m_worldOpaque.isEmpty()))
        worldFrontOpaque += m_worldOpaque;
    commitState();
}

void WSGDamageNode::invalidateCommittedStateRecursive()
{
    m_committedWorldBounds = { };
    m_committedSubtreeAABB = { };
    m_worldBounds = { };
    m_subtreeAABB = { };
    m_worldOpaque = { };
    m_committedVisible = false;
    m_effectiveVisible = false;
    m_subtreeWorldOpaque = { };
    m_commitPending = false;
    for (WSGDamageNode *child = m_firstChild; child; child = child->m_next)
        child->invalidateCommittedStateRecursive();
}

void WSGDamageNode::commitState()
{
    if (!m_effectiveVisible && m_committedVisible) {
        for (WSGDamageNode *child = m_firstChild; child; child = child->m_next)
            child->invalidateCommittedStateRecursive();
    }

    if (m_effectiveVisible) {
        m_committedWorldBounds = hasContent() ? m_worldBounds : QRect();
        m_committedSubtreeAABB = m_subtreeAABB;
    } else {
        m_committedWorldBounds = { };
        m_committedSubtreeAABB = { };
        if (m_deferDirtyWhileInactive) {
            constexpr DirtyBits deferredBits = DirtyMatrix | DirtyGeometry | DirtyContent
                | DirtyAdded | DirtyStructure | DirtyOpaque | DirtySubtree | DirtySubtreeGeometry;
            m_deferredDirty |= m_dirty & deferredBits;
        }
    }
    m_commitPending = false;
    m_committedVisible = m_effectiveVisible;
    m_deferDirtyWhileInactive = false;
    m_dirty = { };
}

void WSGDamageNode::commitStateRecursive()
{
    if (m_effectiveVisible) {
        for (WSGDamageNode *child = m_firstChild; child; child = child->m_next) {
            if (child->m_commitPending)
                child->commitStateRecursive();
        }
    }
    commitState();
}

WSGDamageTransformNode::WSGDamageTransformNode()
    : WSGDamageNode(Type::Transform)
{
}

void WSGDamageTransformNode::setMatrix(const QTransform &matrix)
{
    setMatrix(QMatrix4x4(matrix));
}

void WSGDamageTransformNode::setMatrix(const QMatrix4x4 &matrix)
{
    if (m_matrix == matrix)
        return;
    m_matrix = matrix;
    markDirty(DirtyMatrix);
}

void WSGDamageTransformNode::setTranslation(qreal x, qreal y)
{
    QTransform t;
    t.translate(x, y);
    setMatrix(t);
}

void WSGDamageTransformNode::setScale(qreal sx, qreal sy)
{
    QTransform t;
    t.scale(sx, sy);
    setMatrix(t);
}

void WSGDamageTransformNode::setRotation(qreal degrees, Qt::Axis axis)
{
    QTransform t;
    t.rotate(degrees, axis);
    setMatrix(t);
}

WSGDamageClipNode::WSGDamageClipNode()
    : WSGDamageNode(Type::Clip)
{
}

void WSGDamageClipNode::setClipRect(const QRectF &rect)
{
    if (m_clipRect == rect)
        return;
    m_clipRect = rect;
    markDirty(DirtyGeometry);
}

void WSGDamageClipNode::setIsRectangular(bool rectangular)
{
    if (m_rectangular == rectangular)
        return;
    m_rectangular = rectangular;
    markDirty(DirtyGeometry);
}

void WSGDamageClipNode::setRadius(qreal radius)
{
    if (qFuzzyIsNull(m_radius - radius))
        return;
    m_radius = radius;
    markDirty(DirtyGeometry);
}

WSGDamageGeometryNode::WSGDamageGeometryNode()
    : WSGDamageNode(Type::Geometry)
{
    m_hasContent = true;
}

WSGDamageGeometryNode::WSGDamageGeometryNode(Type type)
    : WSGDamageNode(type)
{
    m_hasContent = true;
}

void WSGDamageGeometryNode::syncFullyOpaqueRegion()
{
    m_opaqueRegion =
        WPixmanRegion(innerAligned(QRectF(0, 0, m_boundingRect.width(), m_boundingRect.height())));
}

void WSGDamageGeometryNode::setBoundingRect(const QRectF &rect)
{
    if (m_boundingRect == rect)
        return;
    m_boundingRect = rect;
    if (m_fullyOpaque)
        syncFullyOpaqueRegion();
    markDirty(DirtyGeometry);
}

void WSGDamageGeometryNode::setOpaqueRegion(const pixman_region32_t *localOpaque)
{
    const WPixmanRegion region(localOpaque);
    if (m_opaqueRegion == region && !m_fullyOpaque)
        return;
    m_fullyOpaque = false;
    m_opaqueRegion = region;
    markDirty(DirtyOpaque);
}

void WSGDamageGeometryNode::setFullyOpaque(bool fullyOpaque)
{
    if (m_fullyOpaque == fullyOpaque && fullyOpaque)
        return;
    if (!fullyOpaque && !m_fullyOpaque && m_opaqueRegion.isEmpty())
        return;
    m_fullyOpaque = fullyOpaque;
    if (fullyOpaque)
        syncFullyOpaqueRegion();
    else
        m_opaqueRegion = { };
    markDirty(DirtyOpaque);
}

void WSGDamageGeometryNode::markContentDirty(const pixman_region32_t *localRegion)
{
    const WPixmanRegion region(localRegion);
    if (region.isEmpty())
        return;
    m_pendingContentDamage += region;
    markDirty(DirtyContent);
}

void WSGDamageGeometryNode::markContentDirty(const QRect &localRect)
{
    if (localRect.isEmpty())
        return;
    m_pendingContentDamage += localRect;
    markDirty(DirtyContent);
}

void WSGDamageGeometryNode::setOpaqueRegion(const QRegion &localOpaque)
{
    setOpaqueRegion(WPixmanRegion::fromQRegion(localOpaque).native());
}

void WSGDamageGeometryNode::markContentDirty(const QRegion &localRegion)
{
    if (localRegion.isEmpty())
        return;
    markContentDirty(WPixmanRegion::fromQRegion(localRegion).native());
}

WSGDamageBackdropNode::WSGDamageBackdropNode()
    : WSGDamageGeometryNode(Type::Geometry)
{
    setNeedsBackdrop(true);
}

void WSGDamageBackdropNode::accumulateRecopy(const pixman_region32_t *damage)
{
    m_recopySeed += damage;
    for (auto it = m_pendingRecopySlots.begin(); it != m_pendingRecopySlots.end(); ++it)
        it.value() += damage;
}

const WPixmanRegion &WSGDamageBackdropNode::pendingRecopy(const void *output) const
{
    Q_ASSERT(output);
    auto it = m_pendingRecopySlots.find(output);
    if (it == m_pendingRecopySlots.end())
        it = m_pendingRecopySlots.insert(output, m_recopySeed);
    return it.value();
}

void WSGDamageBackdropNode::consumeRecopy(const WPixmanRegion &applied, const void *output)
{
    if (applied.isEmpty())
        return;
    auto it = m_pendingRecopySlots.find(output);
    if (it == m_pendingRecopySlots.end())
        return;
    it.value() -= applied;
}

void WSGDamageBackdropNode::releaseRecopy(const void *output)
{
    m_pendingRecopySlots.remove(output);
}

WAYLIB_SERVER_END_NAMESPACE
