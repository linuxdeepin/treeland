// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsgdamagenode.h"

#include <wglobal.h>

#include <QHash>
#include <QMargins>
#include <QMatrix4x4>
#include <QRect>
#include <QRectF>
#include <QSet>
#include <QVarLengthArray>

WAYLIB_SERVER_BEGIN_NAMESPACE

// Per-render-target render parameters and per-frame damage state. The Renderer
// computes the flush region during commit() and hands it back to the caller of
// WBufferRenderer::endRender(); the viewport never stores it.
class WAYLIB_SERVER_EXPORT WSGViewport
{
public:
    WSGViewport() = default;

    explicit WSGViewport(const QRect &outputRect)
        : m_outputRect(outputRect)
    {
    }

    void setOutputRect(const QRect &rect);
    void setRenderParameters(const QMatrix4x4 &renderMatrix,
                             const QRectF &sourceRect = {},
                             const QRectF &targetRect = {});
    QRect outputRect() const
    {
        return m_outputRect;
    }

    const QMatrix4x4 &renderMatrix() const
    {
        return m_renderMatrix;
    }

    QRectF sourceRect() const
    {
        return m_sourceRect;
    }

    QRectF targetRect() const
    {
        return m_targetRect;
    }

    bool isDirty() const
    {
        return m_dirty;
    }

    const pixman_region32_t *outputDamageRegion() const
    {
        return m_outputDamage.native();
    }

    void finishFrame();

private:
    friend class WSGDamageTracker;

    QRect m_outputRect;
    QMatrix4x4 m_renderMatrix;
    QRectF m_sourceRect;
    QRectF m_targetRect;
    bool m_dirty = false;

    WPixmanRegion m_outputDamage;
};

class WAYLIB_SERVER_EXPORT WSGDamageTracker
{
public:
    using Viewport = WSGViewport;
    using BackdropExpansionMap = QHash<quint64, QMargins>;
    enum class Phase
    {
        Idle,
        Prepared,
        Committed
    };

    WSGDamageTracker() = default;
    explicit WSGDamageTracker(WSGDamageNode *root);
    ~WSGDamageTracker();

    void setRoot(WSGDamageNode *root);

    WSGDamageNode *root() const
    {
        return m_root;
    }

    void prepareFrame();
    void commit(WSGViewport &viewport);
    void finishFrame();

    Phase phase() const
    {
        return m_phase;
    }

    bool lastCommitIdle() const
    {
        return m_lastCommitIdle;
    }

    // One viewport-independent walk per frame. Accumulates the frame damage
    // under every backdrop node into the node's own pendingRecopy (scene
    // space, persistent until the renderer consumes it) and fills frameFlush()
    // with the flush content before output clipping. `external` is damage
    // already drawn into the same buffer by sibling renderers this cycle.
    void accumulateFrame(const WPixmanRegion &external);

    const WPixmanRegion &frameFlush() const
    {
        return m_frameFlush;
    }

    bool frameAccumulated() const
    {
        return m_frameAccumulated;
    }

private:
    void walkFrame(WSGDamageNode *n, const QSet<quint64> &frameDirty, WPixmanRegion &current);
    void mapViewport(WSGViewport &viewport);
    void commitViewport(WSGViewport &viewport);
    WSGDamageNode *m_root = nullptr;
    WPixmanRegion m_damage;
    WPixmanRegion m_frameFlush;
    QVarLengthArray<WSGDamageNode *, 64> m_ownDamageNodes;
    Phase m_phase = Phase::Idle;
    bool m_lastCommitIdle = false;
    bool m_occlusionValid = false;
    bool m_frameAccumulated = false;
};


WAYLIB_SERVER_END_NAMESPACE
