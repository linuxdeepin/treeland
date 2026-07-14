// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgdamageobserver_p.h"
#include "wsgdamagetracker_p.h"

#include <QSGRenderNode>

WAYLIB_SERVER_BEGIN_NAMESPACE

WSGDamageObserverRenderer::WSGDamageObserverRenderer(WSGDamageTracker *tracker, QObject *parent)
    : QSGAbstractRenderer(parent)
    , m_tracker(tracker)
{
}

void WSGDamageObserverRenderer::renderScene()
{
    // Observer only — does not render anything. The real QSGRenderer handles
    // rendering via renderNextFrame().
}

void WSGDamageObserverRenderer::nodeChanged(QSGNode *node, QSGNode::DirtyState state)
{
    if (!m_tracker || !node)
        return;

    // DirtyPropagationMask in Qt only covers DirtyMatrix | DirtyNodeAdded |
    // DirtyOpacity | DirtyForceUpdate, so nodeChanged() is only called for
    // these bits. Geometry/material/removed changes do not propagate to the
    // root and thus are not observed here. WSGDamageInfoNode covers surface
    // content changes; the tracker's empty-takeFrameDamage fallback to
    // add_whole() covers any missed cases.
    const QSGNode::DirtyState observedMask =
        QSGNode::DirtyMatrix
        | QSGNode::DirtyNodeAdded;

    if (!(state & observedMask))
        return;

    // Try to get the node's local bounding rect.
    // For QSGRenderNode, rect() gives the local bounding rect.
    // For other node types (TransformNode, OpacityNode, etc.), we cannot
    // easily determine a bounding rect without traversing children, so
    // fall back to reset() (full damage) for correctness.
    if (node->type() == QSGNode::RenderNodeType) {
        auto renderNode = static_cast<QSGRenderNode *>(node);
        QRectF rect = renderNode->rect();
        if (rect.isValid() && !rect.isEmpty()) {
            if (state & QSGNode::DirtyNodeAdded) {
                m_tracker->onNodeAdded(node, rect);
            } else {
                // DirtyMatrix: transform changed — damage old + new root rect.
                m_tracker->onGeometryChanged(node, rect);
            }
            return;
        }
    }

    // Conservative fallback: cannot determine bounding rect → reset tracker
    // to force full damage on next takeFrameDamage().
    m_tracker->reset();
}

WAYLIB_SERVER_END_NAMESPACE
