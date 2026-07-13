// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QHash>
#include <QRegion>
#include <QRectF>
#include <QMatrix4x4>

#include <private/qsgabstractrenderer_p.h>

QT_BEGIN_NAMESPACE
class QSGGeometryNode;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

// Collects QSG nodes reported dirty through QSGRenderer::nodeChanged() (fired
// during QSGRenderContext::sync()) and accumulates their bounding boxes —
// transformed by the accumulated node transform — into a damage region.
//
// Qt 6.8 no longer stores per-node dirty state on QSGNode: the m_dirtyState
// member and dirtyState() are gone, so dirty nodes are gathered from the
// nodeChanged() callback rather than scanned off the tree. The recorded set is
// consumed (and cleared) when the damage region is queried, which must happen
// after sync() and before renderNextFrame() (preprocess) clears the dirty
// flags.
//
// The output region is in the root node's local (logical) coordinate system;
// callers must convert it to buffer pixel coordinates.
//
// Known limitation: DirtyNodeRemoved (and DirtySubtreeBlocked) only damage a
// node's own bounds, not the area its (former) subtree used to occupy, so the
// previous position is not invalidated. That is deferred to a later phase.
class WSGDamageTracker
{
public:
    WSGDamageTracker();

    // Called from QSGRenderer::nodeChanged() to record a node that was marked
    // dirty. The dirty-state bits are accumulated per node.
    void recordDirtyNode(QSGNode *node, QSGNode::DirtyState state);

    // Walk the subtree rooted at \p rootNode and accumulate the bounding boxes
    // of the recorded dirty nodes (and the subtrees of dirty transform/opacity
    // nodes). Returns the damage region in root-node local coordinates and
    // clears the recorded set.
    QRegion consumeDamageRegion(QSGNode *rootNode);

    // Whether any dirty node has been recorded since the last consume.
    bool hasDirtyNodes() const { return !m_dirtyNodes.isEmpty(); }

    // Drop all recorded dirty nodes without computing a region.
    void reset();

private:
    void traverse(QSGNode *node, const QMatrix4x4 &transform, bool damageSubtree);
    static QRectF computeGeometryBoundingRect(const QSGGeometryNode *node);

    QHash<QSGNode *, int> m_dirtyNodes;
    QRegion m_dirtyRegion;
};

// Phase 1 glue: a passive QSGAbstractRenderer registered on a QSGRootNode so
// that QSGRenderer::nodeChanged() callbacks are forwarded to a WSGDamageTracker.
//
// QSGRootNode keeps a QList of registered renderers (see
// QSGAbstractRenderer::setRootNode), so registering this observer alongside the
// real renderer does not displace it. renderScene() is never invoked — the
// observer is only a nodeChanged listener, not a renderer — so it neither
// renders nor emits sceneGraphChanged.
class WSGDirtyNodeObserver : public QSGAbstractRenderer
{
public:
    WSGDirtyNodeObserver() = default;

    WSGDamageTracker &tracker() { return m_tracker; }

    void renderScene() override { }

protected:
    void nodeChanged(QSGNode *node, QSGNode::DirtyState state) override;

private:
    WSGDamageTracker m_tracker;
};

WAYLIB_SERVER_END_NAMESPACE
