// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgdamagetracker_p.h"
#include "wsgimagenode_p.h"

#include <QSGGeometry>
#include <QSGNode>
#include <QSGRenderNode>
#include <QMatrix4x4>
#include <QTransform>

#include <cfloat>

WAYLIB_SERVER_BEGIN_NAMESPACE

namespace {

constexpr qreal kCoordLimit = 1000000.0;

int positionAttributeOffset(QSGGeometry *g)
{
    int vaOffset = 0;
    for (int a = 0; a < g->attributeCount(); ++a) {
        const QSGGeometry::Attribute &attr = g->attributes()[a];
        if (attr.isVertexCoordinate && attr.tupleSize == 2 && attr.type == QSGGeometry::FloatType)
            return vaOffset;
        const int typeSize = attr.type == QSGGeometry::FloatType ? 4
            : attr.type == QSGGeometry::UnsignedByteType ? 1
            : attr.type == QSGGeometry::UnsignedShortType ? 2
            : 4;
        vaOffset += attr.tupleSize * typeSize;
    }
    return -1;
}

QRegion inflate(const QRegion &region, int margin = 1)
{
    if (region.isEmpty())
        return region;
    QRegion out;
    for (const QRect &rect : region)
        out += rect.adjusted(-margin, -margin, margin, margin);
    return out;
}

QRect inflateRect(const QRectF &bounds)
{
    if (bounds.isEmpty())
        return {};
    return bounds.toAlignedRect().adjusted(-1, -1, 1, 1);
}

const QSGNode::DirtyState kStructuralDirty =
    QSGNode::DirtyGeometry | QSGNode::DirtyMatrix | QSGNode::DirtyOpacity
    | QSGNode::DirtyNodeAdded | QSGNode::DirtySubtreeBlocked | QSGNode::DirtyForceUpdate;

} // namespace

void WSGDamageTracker::markFull()
{
    m_fullDamage = true;
    m_pendingDamage = QRegion();
}

void WSGDamageTracker::reset()
{
    m_pendingDamage = QRegion();
    m_flushRegion = QRegion();
    m_lastItemBounds.clear();
    clearNodeDamage();
    m_fullDamage = false;
    m_flushRegionIsFull = false;
}

void WSGDamageTracker::commit()
{
    m_flushRegionIsFull = m_fullDamage;
    m_flushRegion = m_fullDamage ? QRegion() : m_pendingDamage;
    m_pendingDamage = QRegion();
    m_fullDamage = false;
}

void WSGDamageTracker::addRect(const QRect &rect)
{
    if (!rect.isNull() && rect.isValid())
        m_pendingDamage += rect;
}

void WSGDamageTracker::addRegion(const QRegion &region)
{
    if (m_fullDamage || region.isEmpty())
        return;
    m_pendingDamage += region;
}

void WSGDamageTracker::addToFlush(const QRegion &region)
{
    if (m_flushRegionIsFull || region.isEmpty())
        return;
    m_flushRegion += region;
}

void WSGDamageTracker::recordNodeDamage(QSGNode *node, const QRegion &region)
{
    if (!node || m_fullDamage || region.isEmpty())
        return;
    m_nodeDamage[node] += region;
}

QRegion WSGDamageTracker::damageForNode(const QSGNode *node) const
{
    QRegion region;
    for (const QSGNode *n = node; n; n = n->parent()) {
        const auto it = m_nodeDamage.constFind(const_cast<QSGNode *>(n));
        if (it != m_nodeDamage.cend())
            region += *it;
    }
    return region;
}

void WSGDamageTracker::clearNodeDamage()
{
    m_nodeDamage.clear();
    m_removedDamage = QRegion();
}

QMatrix4x4 WSGDamageTracker::combinedMatrix(const QSGNode *node) const
{
    QMatrix4x4 m;
    for (const QSGNode *n = node; n; n = n->parent()) {
        if (n->type() == QSGNode::TransformNodeType)
            m = static_cast<const QSGTransformNode *>(n)->matrix() * m;
    }
    return m;
}

QRectF WSGDamageTracker::itemBounds(QSGNode *node) const
{
    QRectF local;

    switch (node->type()) {
    case QSGNode::GeometryNodeType: {
        auto *gn = static_cast<QSGGeometryNode *>(node);
        QSGGeometry *g = gn->geometry();
        if (!g || g->vertexCount() <= 0)
            return {};
        const int offset = positionAttributeOffset(g);
        if (offset < 0)
            return {};
        qreal minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
        char *vd = static_cast<char *>(g->vertexData()) + offset;
        for (int i = 0; i < g->vertexCount(); ++i) {
            const float *pt = reinterpret_cast<const float *>(vd);
            minX = qMin(minX, qreal(pt[0]));
            minY = qMin(minY, qreal(pt[1]));
            maxX = qMax(maxX, qreal(pt[0]));
            maxY = qMax(maxY, qreal(pt[1]));
            vd += g->sizeOfVertex();
        }
        local = QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
        break;
    }
    case QSGNode::ClipNodeType: {
        local = static_cast<const QSGClipNode *>(node)->clipRect();
        break;
    }
    case QSGNode::RenderNodeType: {
        local = static_cast<const QSGRenderNode *>(node)->rect();
        break;
    }
    default:
        return {};
    }

    if (local.isEmpty())
        return {};
    // Walk current TransformNode matrices. QSGGeometryNode::matrix() (and the
    // same pointer on clip/render nodes) is the combined matrix from the last
    // render, so using it here would report the old position on a move and
    // miss the new one — cursor trails on X11 software cursor.
    const QRectF mapped = combinedMatrix(node).mapRect(local);
    if (mapped.left() < -kCoordLimit || mapped.top() < -kCoordLimit
        || mapped.right() > kCoordLimit || mapped.bottom() > kCoordLimit)
        return {};
    return mapped;
}

QRegion WSGDamageTracker::subtreeItemBounds(QSGNode *node) const
{
    QRegion region;
    const QRect inflated = inflateRect(itemBounds(node));
    if (!inflated.isNull())
        region += inflated;

    for (QSGNode *child = node->firstChild(); child; child = child->nextSibling())
        region += subtreeItemBounds(child);
    return region;
}

QRegion WSGDamageTracker::mappedDamageRegion(WSGImageNode *node) const
{
    if (!node->hasExplicitDamage())
        return {};
    const QMatrix4x4 matrix = combinedMatrix(node);
    if (!matrix.isAffine())
        return {};
    return inflate(matrix.toTransform().map(node->damageRegion()));
}

void WSGDamageTracker::dropCachedBounds(QSGNode *node)
{
    m_lastItemBounds.remove(node);
    for (QSGNode *child = node->firstChild(); child; child = child->nextSibling())
        dropCachedBounds(child);
}

void WSGDamageTracker::cacheSubtreeBounds(QSGNode *node)
{
    // Qt notifies DirtyNodeAdded only on the inserted subtree root
    // (setRootNode, or a parent appending an already-built itemNode).
    // Walk descendants so nested TransformNodes keep an AABB, including
    // empty Overlay-style nodes that later gain children.
    QRegion region;
    const QRect local = inflateRect(itemBounds(node));
    if (!local.isNull())
        region += local;
    for (QSGNode *child = node->firstChild(); child; child = child->nextSibling()) {
        cacheSubtreeBounds(child);
        const QRect childBounds = m_lastItemBounds.value(child);
        if (!childBounds.isNull())
            region += childBounds;
    }
    m_lastItemBounds.insert(node, region.boundingRect());
}

QRegion WSGDamageTracker::liveBounds(QSGNode *node) const
{
    QRegion region = subtreeItemBounds(node);
    if (region.isEmpty()) {
        const QRect inflated = inflateRect(itemBounds(node));
        if (!inflated.isNull())
            region += inflated;
    }
    return region;
}

void WSGDamageTracker::nodeChanged(QSGNode *node, QSGNode::DirtyState state)
{
    if (state & QSGNode::DirtyNodeRemoved) {
        QRect prev = m_lastItemBounds.value(node);
        // Parent is still set (QSGNode::removeChildNode notifies before
        // clearing it). Live AABB is the last on-screen rect — enough to
        // erase a blinking caret without a full-output fallback.
        if (prev.isNull()) {
            const QRegion live = liveBounds(node);
            if (!live.isEmpty())
                prev = live.boundingRect();
        }
        dropCachedBounds(node);
        if (!m_fullDamage && !prev.isNull()) {
            addRect(prev);
            m_removedDamage += prev;
        }
        return;
    }

    if (state & QSGNode::DirtyForceUpdate) {
        const QRect prev = m_lastItemBounds.value(node);
        cacheSubtreeBounds(node);
        QRegion region = liveBounds(node);
        if (!prev.isNull())
            region += prev;
        if (m_fullDamage)
            return;
        if (region.isEmpty()) {
            if (node->type() == QSGNode::RootNodeType)
                markFull();
        } else {
            m_pendingDamage += region;
            recordNodeDamage(node, region);
        }
        return;
    }

    // Content updates on a WSGImageNode report only the explicit region
    // (empty = no content damage). Moves/resizes still use geometry AABBs.
    const bool materialOnly = (state & QSGNode::DirtyMaterial) && !(state & kStructuralDirty);
    if (materialOnly) {
        if (WSGImageNode *image = WSGImageNode::enclosingNode(node); image && image->hasExplicitDamage()) {
            if (!m_fullDamage) {
                const QRegion mapped = mappedDamageRegion(image);
                m_pendingDamage += mapped;
                recordNodeDamage(image, mapped);
            }
            return;
        }
    }

    QRegion now;
    const bool affectsSubtree = state & (QSGNode::DirtyMatrix
                                         | QSGNode::DirtyOpacity
                                         | QSGNode::DirtySubtreeBlocked
                                         | QSGNode::DirtyNodeAdded);
    if (affectsSubtree
        && (node->type() == QSGNode::TransformNodeType
            || node->type() == QSGNode::OpacityNodeType
            || node->type() == QSGNode::RootNodeType)) {
        now = subtreeItemBounds(node);
    } else if (state & (QSGNode::DirtyGeometry | QSGNode::DirtyMaterial | QSGNode::DirtyNodeAdded
                        | QSGNode::DirtyMatrix | QSGNode::DirtyOpacity
                        | QSGNode::DirtySubtreeBlocked)) {
        const QRect inflated = inflateRect(itemBounds(node));
        if (!inflated.isNull())
            now += inflated;
    }

    // TransformNode / text node DirtyMaterial has no local geometry; the
    // caret (or other children) still have a bounded AABB.
    if (now.isEmpty())
        now = liveBounds(node);

    const QRect prev = m_lastItemBounds.value(node);
    if (state & QSGNode::DirtyNodeAdded) {
        cacheSubtreeBounds(node);
        for (QSGNode *ancestor = node->parent(); ancestor; ancestor = ancestor->parent()) {
            if (ancestor->type() != QSGNode::TransformNodeType
                && ancestor->type() != QSGNode::OpacityNodeType
                && ancestor->type() != QSGNode::RootNodeType) {
                continue;
            }
            const QRegion ancestorBounds = subtreeItemBounds(ancestor);
            m_lastItemBounds.insert(ancestor, ancestorBounds.boundingRect());
        }
    } else if (!now.isEmpty()) {
        m_lastItemBounds.insert(node, now.boundingRect());
    }
    // First frame often marks the root full before children are Added.
    // Keep AABBs anyway so the next move can erase old pixels.
    if (m_fullDamage)
        return;

    addRect(prev);

    if (now.isEmpty()) {
        if (!prev.isNull())
            recordNodeDamage(node, QRegion(prev));
        return;
    }

    m_pendingDamage += now;
    QRegion reported = now;
    if (!prev.isNull())
        reported += prev;
    recordNodeDamage(node, reported);
}

WAYLIB_SERVER_END_NAMESPACE
