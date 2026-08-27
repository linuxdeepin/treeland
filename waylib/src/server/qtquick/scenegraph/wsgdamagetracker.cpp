// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "wsgdamagetracker_p.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WSGDamageTracker::WSGDamageTracker(WSGDamageNode *root)
    : m_root(root)
{
}

WSGDamageTracker::~WSGDamageTracker() = default;

void WSGDamageTracker::setRoot(WSGDamageNode *root)
{
    if (m_root == root)
        return;
    m_root = root;
    m_committed = false;
    m_damage = { };
}

WSGDamageNode *WSGDamageTracker::root() const
{
    return m_root;
}

void WSGDamageTracker::settle()
{
    if (m_committed)
        return;
    m_committed = true;
    m_damage = { };
    if (m_root && m_root->isDirty()) {
        WPixmanRegion worldDamage;
        WPixmanRegion backdropDamage;
        m_root->updateWorld(QMatrix4x4(), false, worldDamage, backdropDamage);
        m_damage = worldDamage;
        m_damage += backdropDamage;

        constexpr auto kShape = WSGDamageNode::DirtyMatrix | WSGDamageNode::DirtyGeometry
            | WSGDamageNode::DirtyAdded | WSGDamageNode::DirtyStructure
            | WSGDamageNode::DirtyVisibility | WSGDamageNode::DirtyOpaque
            | WSGDamageNode::DirtySubtreeGeometry;
        if (m_root->dirty() & kShape) {
            WPixmanRegion worldFrontOpaque;
            WPixmanRegion worldFrontBackdrop;
            m_root->computeWorldVisibility(worldFrontOpaque, worldFrontBackdrop);
        } else {
            m_root->commitStateRecursive();
        }
    }
}

void WSGDamageTracker::commit(WSGViewport &viewport)
{
    if (viewport.isFull())
        return;
    settle();
    if (m_root && !m_damage.isEmpty())
        viewport.addDamage(WDamageRegion(m_damage, false));
}


void WSGDamageTracker::finishFrame()
{
    if (!m_committed)
        settle();
    m_committed = false;
}

WAYLIB_SERVER_END_NAMESPACE
