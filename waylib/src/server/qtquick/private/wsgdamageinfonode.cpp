// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgdamageinfonode_p.h"
#include "wsgdamagetracker_p.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WSGDamageInfoNode::WSGDamageInfoNode()
{
    setFlag(QSGNode::OwnedByParent);
    setFlag(QSGNode::UsePreprocess);
}

void WSGDamageInfoNode::setDamage(const QRegion &damage)
{
    m_damage += damage;
}

void WSGDamageInfoNode::setTracker(WSGDamageTracker *tracker)
{
    m_tracker = tracker;
}

void WSGDamageInfoNode::setNodeId(const void *nodeId)
{
    m_nodeId = nodeId;
}

void WSGDamageInfoNode::preprocess()
{
    if (m_tracker && m_nodeId && !m_damage.isEmpty()) {
        m_tracker->onContentDirty(m_nodeId, m_damage);
    }
    m_damage = QRegion();
}

WAYLIB_SERVER_END_NAMESPACE
