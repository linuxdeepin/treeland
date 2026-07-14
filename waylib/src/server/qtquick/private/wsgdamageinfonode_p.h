// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QRegion>
#include <QSGNode>

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
// during the scene graph preprocess phase (render thread, before rendering).
// In preprocess(), the node reports its damage to the associated WSGDamageTracker
// via onContentDirty(), then clears its damage (consumed once per frame).
//
// Coordinate system:
//   - The damage region is in the parent node's local coordinate system
//     (i.e., the surface content's local coordinates).
//   - WSGDamageTracker maps it to root-local coordinates via the node's
//     transform chain (set by Layer 3b observer or explicit registration).
//
// Lifecycle:
//   - Created in updatePaintNode alongside WSGRenderFootprintNode.
//   - Owned by the parent QSGNode (OwnedByParent flag).
//   - Damage is set by WSurfaceItemContent on surface commit, consumed by
//     preprocess() on the next render frame.
class WAYLIB_SERVER_EXPORT WSGDamageInfoNode : public QSGNode
{
public:
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

    // QSGNode override — called by QSGRenderer during preprocess phase.
    void preprocess() override;

private:
    QRegion m_damage;
    WSGDamageTracker *m_tracker = nullptr;
    const void *m_nodeId = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
