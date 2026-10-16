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


    // Walks the dirty tree, copies scene damage into every registered
    // non-full viewport, then unlocks so the next commit() can settle
    // new dirty. Mapping to a specific buffer happens at draw time.
    void commit();

private:
    void settle();

    WSGDamageNode *m_root = nullptr;
    WPixmanRegion m_damage;
    QList<std::weak_ptr<WSGViewport>> m_viewports;
    bool m_settled = false;
};

WAYLIB_SERVER_END_NAMESPACE
