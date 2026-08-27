// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wsgdamagenode_p.h"
#include "wsgviewport_p.h"
#include <wglobal.h>
#include <QList>
#include <memory>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WSGDamageTracker
{
public:
    WSGDamageTracker() = default;
    explicit WSGDamageTracker(WSGDamageNode *root);
    ~WSGDamageTracker();

    void setRoot(WSGDamageNode *root);
    WSGDamageNode *root() const;

    void addViewport(const std::shared_ptr<WSGViewport> &viewport);
    void removeViewport(WSGViewport *viewport);
    void accumulateBackdropRecopy(const WSGDamageBackdropNode *node, const pixman_region32_t *damage);


    // Walks the dirty tree at most once per round, then copies scene damage
    // into every registered non-full viewport unclipped. Mapping to a specific
    // buffer happens at draw time. Subsequent calls in the same round are no-ops.
    void commit();
    // Ends the round. Idempotent. Clears the frame damage accumulator.
    void finishFrame();

private:
    // Walks the dirty tree at most once per round: world bounds, occlusion,
    // and commitState(). Idempotent until finishFrame(). Does not write any
    // viewport.
    void settle();

    WSGDamageNode *m_root = nullptr;
    WPixmanRegion m_damage;
    QList<std::weak_ptr<WSGViewport>> m_viewports;
    bool m_committed = false;
};

WAYLIB_SERVER_END_NAMESPACE
