// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsgdamagenode_p.h"
#include "wsgviewport_p.h"

#include <wglobal.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WSGDamageTracker
{
public:
    WSGDamageTracker() = default;
    explicit WSGDamageTracker(WSGDamageNode *root);
    ~WSGDamageTracker();

    void setRoot(WSGDamageNode *root);
    WSGDamageNode *root() const;

    // Walks the dirty tree at most once per round, then copies scene damage
    // into the viewport unclipped. Mapping to a specific buffer happens at
    // draw time. Full viewports skip: they already redraw everything. Every
    // non-full viewport sharing this scene must commit before finishFrame()
    // so slower outputs keep damage the faster ones already consumed.
    void commit(WSGViewport &viewport);
    // Ends the round. If no commit() ran, still settle the tree so dirty
    // flags and world bounds do not leak into the next frame. Does not
    // write any viewport.
    void finishFrame();

private:
    // Walks the dirty tree at most once per round: world bounds, occlusion,
    // and commitState(). Idempotent until finishFrame(). Does not write any
    // viewport.
    void settle();

    WSGDamageNode *m_root = nullptr;
    WPixmanRegion m_damage;
    bool m_committed = false;
};


WAYLIB_SERVER_END_NAMESPACE
