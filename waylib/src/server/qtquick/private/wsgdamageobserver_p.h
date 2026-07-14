// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QRectF>
#include <private/qsgabstractrenderer_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSGDamageTracker;

// WSGDamageObserverRenderer — Layer 3b: passive QSG node change observer.
//
// A QSGAbstractRenderer subclass that mounts to the same QSGRootNode as the
// real renderer. It does not render anything (renderScene() is a no-op) —
// its sole purpose is to receive nodeChanged() callbacks from the scene graph
// and forward structural/transform changes to the WSGDamageTracker.
//
// QSGRootNode::notifyNodeChange calls nodeChanged() on all attached renderers,
// so the observer runs alongside the real renderer without interfering.
//
// nodeChanged() is called during the scene graph sync phase (GUI thread,
// blocked while render thread processes the scene). The tracker is accessed
// within the controlled QQuickRenderControl sync→preprocess→render sequence,
// so no additional locking is needed.
//
// Bounding rect strategy: For QSGRenderNode, use rect(). For other node types
// or when the bounding rect cannot be determined, call tracker->reset() to
// fall back to full damage (add_whole). This conservative approach ensures
// correctness; precision can be improved in later iterations.
class WAYLIB_SERVER_EXPORT WSGDamageObserverRenderer : public QSGAbstractRenderer
{
    Q_OBJECT

public:
    explicit WSGDamageObserverRenderer(WSGDamageTracker *tracker, QObject *parent = nullptr);
    ~WSGDamageObserverRenderer() override = default;

    void renderScene() override;

protected:
    void nodeChanged(QSGNode *node, QSGNode::DirtyState state) override;

private:
    WSGDamageTracker *m_tracker;
};

WAYLIB_SERVER_END_NAMESPACE
