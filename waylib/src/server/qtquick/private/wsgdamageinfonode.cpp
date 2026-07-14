// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgdamageinfonode_p.h"
#include "wsgdamagetracker_p.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WSGDamageInfoNode::WSGDamageInfoNode()
    : QSGNode(Type)
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

void WSGDamageInfoNode::syncRegistration(WSGDamageTracker *tracker,
                                          const QTransform &localToRoot,
                                          const QRectF &localRect)
{
    if (!tracker || !m_nodeId)
        return;

    // Re-register if the tracker was reset (e.g. rootNode change) or this
    // is the first call.
    if (!m_registered || !tracker->hasNode(m_nodeId)) {
        tracker->setNodeParent(m_nodeId, nullptr);
        tracker->setNodeTransform(m_nodeId, localToRoot);
        tracker->onNodeAdded(m_nodeId, localRect);
        m_registered = true;
    } else {
        if (m_lastTransform != localToRoot)
            tracker->onTransformChanged(m_nodeId, localToRoot);
        if (m_lastRect != localRect)
            tracker->onGeometryChanged(m_nodeId, localRect);
    }

    m_lastTransform = localToRoot;
    m_lastRect = localRect;
}

void WSGDamageInfoNode::notifyRemoved(WSGDamageTracker *tracker)
{
    if (tracker && m_nodeId && m_registered) {
        tracker->onNodeRemoved(m_nodeId);
        m_registered = false;
    }
}

void WSGDamageInfoNode::preprocess()
{
    if (m_tracker && m_nodeId && !m_damage.isEmpty()) {
        m_tracker->onContentDirty(m_nodeId, m_damage);
    }
    m_damage = QRegion();
}

WAYLIB_SERVER_END_NAMESPACE
