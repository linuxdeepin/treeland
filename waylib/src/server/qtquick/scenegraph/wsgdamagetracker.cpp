// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgdamagetracker.h"

#include <QVarLengthArray>

#include <cmath>

WAYLIB_SERVER_BEGIN_NAMESPACE


static bool occlusionNeedsRefresh(const WSGDamageNode *n)
{
    const WSGDamageNode::DirtyBits d = n->dirty();
    constexpr auto kShape = WSGDamageNode::DirtyMatrix | WSGDamageNode::DirtyGeometry
        | WSGDamageNode::DirtyAdded | WSGDamageNode::DirtyStructure | WSGDamageNode::DirtyVisibility
        | WSGDamageNode::DirtyOpaque | WSGDamageNode::DirtySubtreeGeometry;
    if (d & kShape)
        return true;
    if (!(d & WSGDamageNode::DirtySubtree))
        return false;
    for (const WSGDamageNode *c = n->firstChild(); c; c = c->nextSibling()) {
        if (occlusionNeedsRefresh(c))
            return true;
    }
    return false;
}

void WSGViewport::setOutputRect(const QRect &rect)
{
    if (m_outputRect == rect)
        return;
    m_outputRect = rect;
    m_dirty = true;
}


void WSGViewport::setRenderParameters(const QMatrix4x4 &renderMatrix,
                                      const QRectF &sourceRect,
                                      const QRectF &targetRect)
{
    m_renderMatrix = renderMatrix;
    m_sourceRect = sourceRect;
    m_targetRect = targetRect;
}


void WSGViewport::finishFrame()
{
    m_dirty = false;
    m_outputDamage = { };
}

WSGDamageTracker::WSGDamageTracker(WSGDamageNode *root)
    : m_root(root)
{
}

WSGDamageTracker::~WSGDamageTracker() = default;

void WSGDamageTracker::setRoot(WSGDamageNode *root)
{
    m_root = root;
}

void WSGDamageTracker::prepareFrame()
{
    if (m_phase != Phase::Idle)
        return; // Already prepared for this frame (window-driven lifecycle).
    m_phase = Phase::Prepared;
    m_damage = { };
    m_ownDamageNodes.clear();
    m_occlusionValid = false;
    m_frameAccumulated = false;
    m_frameFlush = { };
    if (Q_UNLIKELY(!m_root))
        return;
    if (Q_LIKELY(!m_root->isDirty())) {
        // behindDamage is filled by updateWorld and is not read on the
        // compositor path (recapture uses accumulateFlush). Leave last-frame
        // values; the next dirty updateWorld overwrites them.
        return;
    }
    WPixmanRegion backdropDamage;
    m_root->updateWorld(QTransform(),
                        false,
                        m_damage,
                        backdropDamage,
                        nullptr,
                        nullptr,
                        &m_ownDamageNodes);
    m_damage += backdropDamage;
}

void WSGDamageTracker::mapViewport(WSGViewport &vp)
{
    const QRect treeBounds = m_root ? m_root->subtreeBounds() : QRect();
    WPixmanRegion frameDamage(m_damage);
    if (vp.m_dirty)
        frameDamage += treeBounds;
    if (!vp.m_outputRect.isEmpty())
        frameDamage &= vp.m_outputRect;
    vp.m_outputDamage += frameDamage;
}


void WSGDamageTracker::commit(WSGViewport &viewport)
{
    if (m_phase == Phase::Idle)
        prepareFrame(); // Late/secondary renderer: self-prime this frame.
    commitViewport(viewport);
}

void WSGDamageTracker::commitViewport(WSGViewport &viewport)
{
    m_phase = Phase::Committed;
    m_lastCommitIdle = false;
    if (Q_UNLIKELY(!m_root))
        return;

    bool idle = !m_root->isDirty() && m_damage.isEmpty();
    if (idle && viewport.m_dirty)
        idle = false;
    if (Q_LIKELY(idle)) {
        // World bounds/opaque/valid did not change. Skip the front-to-back
        // visibility walk; last commit's worldValidRegion is still correct.
        m_lastCommitIdle = true;
        return;
    }

    // Visibility/occlusion is viewport-independent: walk the tree once per
    // frame and reuse the result for every viewport committed afterwards.
    if (!m_occlusionValid) {
        if (occlusionNeedsRefresh(m_root)) {
            WPixmanRegion worldFrontOpaque;
            WPixmanRegion worldFrontBackdrop;
            m_root->computeWorldVisibility(worldFrontOpaque, worldFrontBackdrop);
        }
        m_occlusionValid = true;
    }

    mapViewport(viewport);
}

void WSGDamageTracker::finishFrame()
{
    // The window drives this at frame end. A tracker that was prepared but
    // never committed (its renderer skipped the draw) still needs commitState
    // to settle the world state and clear the dirty flags.
    if (m_phase == Phase::Idle)
        return;
    m_phase = Phase::Idle;
    if (m_root && m_root->isDirty())
        m_root->commitState();
}

// One viewport-independent walk per frame. Paint-order threading mirrors the
// old accumulateFlush: content ownDamage lands before children so a glass
// child can sample it; grouping-node holes (removed descendants) land before
// children too so backdrop children see them, with an idempotent re-add after
// children restoring what an opaque child's worldOpaque subtraction removes.
// Each backdrop node accumulates the damage under it into its own
// pendingRecopy; the glass redraw area joins the flush via `extra`.
void WSGDamageTracker::accumulateFrame(const WPixmanRegion &external)
{
    m_frameAccumulated = true;
    m_frameFlush = external;
    if (!m_root)
        return;
    QSet<quint64> frameDirty;
    frameDirty.reserve(m_ownDamageNodes.size());
    for (WSGDamageNode *node : m_ownDamageNodes) {
        if (!node->m_ownDamage.isEmpty())
            frameDirty.insert(node->id());
    }
    walkFrame(m_root, frameDirty, m_frameFlush);
}

void WSGDamageTracker::walkFrame(WSGDamageNode *n,
                                 const QSet<quint64> &frameDirty,
                                 WPixmanRegion &current)
{
    if (!n)
        return;
    // ownDamage persists after commitState; only nodes that contributed to
    // this frame's world update may contribute here.
    const pixman_region32_t *own =
        frameDirty.contains(n->id()) ? WSGDamageNodeTestAccess::ownDamage(n) : nullptr;
    if (!n->isVisible()) {
        current += own;
        return;
    }

    WPixmanRegion extra;
    if (n->needsBackdrop() && n->hasContent()) {
        WPixmanRegion source(n->worldBounds());
        source &= current;
        if (!source.isEmpty()) {
            if (auto *backdrop = n->toBackdrop()) {
                backdrop->accumulateRecopy(source.native());
                const QMargins &expansion = backdrop->recopyExpansion();
                extra = expansion.isNull() ? WPixmanRegion(source)
                                           : dilateRegion(source, expansion);
                extra &= n->worldBounds();
            }
        }
    }
    if (n->hasContent())
        current -= n->worldOpaqueRegion();
    if (n->hasContent())
        current += own;
    current += extra;
    if (!n->hasContent())
        current += own;
    for (WSGDamageNode *child = n->firstChild(); child; child = child->nextSibling())
        walkFrame(child, frameDirty, current);
    if (!n->hasContent())
        current += own;
}

WAYLIB_SERVER_END_NAMESPACE
