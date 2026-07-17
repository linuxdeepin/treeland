// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QRectF>
#include <QRegion>
#include <QSGNode>
#include <QTransform>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSGDamageTracker;

// WSGDamageInfoNode — Layer 3a: QSGNode carrying refined surface damage.
//
// A lightweight QSGNode subclass that attaches as a child of an image node
// (or any content node) in WSurfaceItemContent::updatePaintNode. It carries
// a QRegion describing the refined damage of its parent node, sourced from
// the wayland surface's get_effective_damage.
//
// The node sets QSGNode::UsePreprocess so that QSGRenderer calls preprocess()
// during the scene graph preprocess phase (before rendering).
// In preprocess(), the node reports its damage to the associated WSGDamageTracker
// via onContentDirty(), then clears its damage (consumed once per frame).
//
// Coordinate system:
//   - The damage region is in the parent node's local coordinate system
//     (i.e., the surface content's local coordinates).
//   - WSGDamageTracker maps it to root-local coordinates via the node's
//     transform chain (set by syncRegistration()).
//
// Lifecycle:
//   - Created in updatePaintNode alongside WSGRenderFootprintNode.
//   - Owned by the parent QSGNode (OwnedByParent flag).
//   - Damage is set by WSurfaceItemContent on surface commit, consumed by
//     preprocess() on the next render frame.
class WAYLIB_SERVER_EXPORT WSGDamageInfoNode : public QSGNode
{
public:
    // Custom type value above Qt's built-in NodeType range (max = RenderNodeType)
    // for type()-based identification without dynamic_cast.
    static constexpr NodeType Type = static_cast<NodeType>(256);

    explicit WSGDamageInfoNode();
    ~WSGDamageInfoNode() override = default;

    // Set the damage region for this frame (in parent node local coordinates).
    // Called by WSurfaceItemContent when the wayland surface commits new damage.
    // Accumulates if called multiple times before preprocess consumes it.
    void setDamage(const QRegion &damage);

    // Associate a tracker that will receive damage reports in preprocess().
    // Pass nullptr to disassociate (e.g., when the output/renderer changes).
    void setTracker(WSGDamageTracker *tracker);

    // Set the node id used by the tracker to identify this node's parent.
    // Typically the parent QSGNode pointer.
    void setNodeId(const void *nodeId);

    // Register or update the parent node in the tracker. Called from
    // updatePaintNode with the node's local-to-root transform and local rect.
    // On first call, registers the node via setNodeParent/setNodeTransform/
    // onNodeAdded. On subsequent calls, only fires onTransformChanged/
    // onGeometryChanged when values actually change, avoiding spurious damage.
    void syncRegistration(WSGDamageTracker *tracker,
                          const QTransform &localToRoot,
                          const QRectF &localRect);

    // Notify the tracker that the parent node is being removed (deleted).
    // Called before deleting the parent QSGNode so the tracker can clean up
    // and damage the node's last known area.
    void notifyRemoved(WSGDamageTracker *tracker);

    // QSGNode override — called by QSGRenderer during the preprocess phase.
    void preprocess() override;

private:
    QRegion m_damage;
    WSGDamageTracker *m_tracker = nullptr;
    const void *m_nodeId = nullptr;

    // Registration state — tracks the last transform/rect reported to the
    // tracker so we only fire change events when something actually moved.
    QTransform m_lastTransform;
    QRectF m_lastRect;
    bool m_registered = false;
};

WAYLIB_SERVER_END_NAMESPACE
