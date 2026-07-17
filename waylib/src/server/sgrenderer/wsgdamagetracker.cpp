// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgdamagetracker.h"

#include <QSGNode>
#include <QSGGeometryNode>
#include <QSGTransformNode>
#include <QSGRenderNode>
#include <QSGGeometry>

#include <algorithm>
#include <limits>

WAYLIB_SERVER_BEGIN_NAMESPACE

// Dirty states whose presence on a node means its visual area must be
// invalidated. DirtyMatrix is included so transform changes (window move,
// animation, scroll) damage the affected subtree.
//
// DirtyNodeRemoved is excluded on purpose: a removed node is detached from the
// tree before the scan runs, so its previous bounds cannot be derived from the
// tree walk. Invalidating the former position is deferred to a later phase (see
// WSGDamageTracker's class docs). DirtySubtreeBlocked shares this limitation
// for the old visible area of a now-hidden subtree.
static constexpr int DirtyDamageMask =
    QSGNode::DirtyGeometry
    | QSGNode::DirtyMaterial
    | QSGNode::DirtyOpacity
    | QSGNode::DirtyForceUpdate
    | QSGNode::DirtyNodeAdded
    | QSGNode::DirtySubtreeBlocked
    | QSGNode::DirtyMatrix;

WSGDamageTracker::WSGDamageTracker() = default;

void WSGDamageTracker::reset()
{
    m_dirtyNodes.clear();
    m_dirtyRegion = QRegion();
}

void WSGDamageTracker::recordDirtyNode(QSGNode *node, QSGNode::DirtyState state)
{
    if (!node)
        return;
    const int bits = static_cast<int>(state);
    if (bits)
        m_dirtyNodes[node] |= bits;
}

void WSGDamageTracker::traverse(QSGNode *node, const QMatrix4x4 &transform,
                                bool damageSubtree)
{
    if (!node)
        return;

    const int dirtyBits = m_dirtyNodes.value(node, 0);
    const bool nodeDirty = (dirtyBits & DirtyDamageMask) != 0;
    const bool shouldDamage = damageSubtree || nodeDirty;

    // Accumulate the transform so child geometry bounding boxes are mapped to
    // the right location.
    QMatrix4x4 currentTransform = transform;
    if (node->type() == QSGNode::TransformNodeType) {
        auto *tn = static_cast<QSGTransformNode*>(node);
        currentTransform = currentTransform * tn->matrix();
    }

    // Add the bounding box for dirty geometry/render nodes.
    if (shouldDamage) {
        QRectF rect;
        if (node->type() == QSGNode::GeometryNodeType) {
            rect = computeGeometryBoundingRect(static_cast<QSGGeometryNode*>(node));
        } else if (node->type() == QSGNode::RenderNodeType) {
            rect = static_cast<QSGRenderNode*>(node)->rect();
        }

        if (rect.isValid() && !rect.isEmpty()) {
            const QRectF mapped = currentTransform.mapRect(rect);
            if (mapped.isValid() && !mapped.isEmpty())
                m_dirtyRegion += mapped.toAlignedRect();
        }
    }

    // Skip children of blocked subtrees — they will not be rendered.
    if (node->isSubtreeBlocked())
        return;

    // Propagate the damage flag so geometry nodes under a dirty
    // transform/opacity/clip node also get their bounding boxes added.
    const bool childDamage = shouldDamage;
    for (QSGNode *child = node->firstChild(); child; child = child->nextSibling())
        traverse(child, currentTransform, childDamage);
}

QRegion WSGDamageTracker::consumeDamageRegion(QSGNode *rootNode)
{
    m_dirtyRegion = QRegion();
    if (rootNode && !m_dirtyNodes.isEmpty())
        traverse(rootNode, QMatrix4x4(), false);
    m_dirtyNodes.clear();
    return m_dirtyRegion;
}

QRectF WSGDamageTracker::computeGeometryBoundingRect(const QSGGeometryNode *node)
{
    const QSGGeometry *g = node->geometry();
    if (!g || g->vertexCount() <= 0 || g->sizeOfVertex() <= 0)
        return {};

    const char *data = static_cast<const char*>(g->vertexData());
    if (!data)
        return {};

    const int stride = g->sizeOfVertex();
    const int count = g->vertexCount();

    // All standard QSGGeometry vertex formats (Point2D, ColoredPoint2D,
    // TexturedPoint2D) store x,y as the first two floats of each vertex.
    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();

    for (int i = 0; i < count; ++i) {
        const float *p = reinterpret_cast<const float*>(data + i * stride);
        const float x = p[0];
        const float y = p[1];
        if (x < minX) minX = x;
        if (y < minY) minY = y;
        if (x > maxX) maxX = x;
        if (y > maxY) maxY = y;
    }

    if (minX > maxX || minY > maxY)
        return {};

    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

void WSGDirtyNodeObserver::nodeChanged(QSGNode *node, QSGNode::DirtyState state)
{
    m_tracker.recordDirtyNode(node, state);
}

WAYLIB_SERVER_END_NAMESPACE
