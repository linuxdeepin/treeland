// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QHash>
#include <QList>
#include <QRegion>
#include <QTransform>

WAYLIB_SERVER_BEGIN_NAMESPACE

// WSGDamageTracker — Layer 2: pure data-structure damage tracker.
//
// Manages a tree of rectangles and computes a QRegion damage area when nodes
// are added, removed, moved, resized, or have their content refreshed.  This
// is a general-purpose capability with no dependency on QSG, GPU, or Qt Quick —
// only QtCore/QtGui (QRegion, QTransform, QRectF).
//
// Coordinate system:
//   - Each node has a localRect (in its own local coordinate space) and a
//     transform that maps local coordinates to its parent's coordinate space.
//   - The tracker maps all geometry to root-local coordinates internally and
//     accumulates damage in root-local coordinates.
//   - takeFrameDamage() returns the accumulated damage in root-local coords.
//
// Usage (Layer 3 will drive this from QSG events):
//   tracker.setNodeParent(nodeA, nullptr);       // root
//   tracker.setNodeTransform(nodeA, QTransform());
//   tracker.onNodeAdded(nodeA, QRectF(0,0,100,100));
//   // ... frame renders ...
//   QRegion damage = tracker.takeFrameDamage();  // (0,0,100,100)
//   // ... next frame, move nodeA ...
//   tracker.onGeometryChanged(nodeA, QRectF(20,0,100,100));
//   damage = tracker.takeFrameDamage();           // (0,0,120,100)
class WAYLIB_SERVER_EXPORT WSGDamageTracker
{
public:
    // Opaque node identifier. Layer 3 uses QSGNode*; tests use arbitrary pointers.
    using NodeId = const void *;

    WSGDamageTracker() = default;
    ~WSGDamageTracker() = default;

    // --- Tree structure ---

    // Set the parent of a node. Pass nullptr for root-level nodes.
    // Must be called before onNodeAdded() so the transform chain is correct.
    void setNodeParent(NodeId node, NodeId parent);

    // Set the local-to-parent transform for a node.
    // Identity transform by default.
    // Must be called before onNodeAdded() so the transform chain is correct.
    void setNodeTransform(NodeId node, const QTransform &transform);

    // --- Event-driven API ---
    // All rects and regions are in the node's LOCAL coordinate system.
    // The tracker maps them to root-local coordinates internally.

    // A new node appeared with the given local rect.
    void onNodeAdded(NodeId node, const QRectF &localRect);

    // A node was removed; its last known root-local rect is damaged.
    void onNodeRemoved(NodeId node);

    // A node's local rect changed (geometry update).
    // Both old and new root-local rects are damaged.
    void onGeometryChanged(NodeId node, const QRectF &newLocalRect);

    // A node's content was refreshed in a sub-region (refined damage from
    // WSGDamageInfoNode / wayland surface damage).  The subRegion is in the
    // node's local coordinates and is mapped to root-local by the tracker.
    void onContentDirty(NodeId node, const QRegion &localSubRegion);

    // A node's local-to-parent transform changed.
    // Both old and new root-local rects are damaged.
    void onTransformChanged(NodeId node, const QTransform &newTransform);

    // --- Output ---

    // Returns the accumulated frame damage in root-local coordinates and
    // clears the internal accumulator.  Returns an empty region if no damage
    // occurred or after reset() — the caller should fall back to add_whole().
    QRegion takeFrameDamage();

    // Invalidate the entire tracker state.  After reset(), takeFrameDamage()
    // returns an empty region, signalling the caller to do a full refresh.
    // Layer 3 must re-register nodes on subsequent frames.
    void reset();

private:
    struct NodeData {
        QRectF localRect;
        QTransform transform;     // local-to-parent
        NodeId parent = nullptr;
        QRectF lastRootRect;      // cached root-local rect from last event
    };

    QHash<NodeId, NodeData> m_nodes;
    QRegion m_frameDamage;

    // Accumulate the transform chain from node to root.
    QTransform nodeToRootTransform(NodeId node) const;
    // Map a local rect to root-local coordinates.
    QRectF mapToRoot(NodeId node, const QRectF &localRect) const;
    // Map a local region to root-local coordinates.
    QRegion mapToRoot(NodeId node, const QRegion &localRegion) const;

    // Recursively update lastRootRect for all descendants of the given node
    // and damage their old + new root rects. Called after an ancestor's
    // transform or geometry changes to keep descendant caches in sync.
    void updateDescendants(NodeId parent);
};

WAYLIB_SERVER_END_NAMESPACE
