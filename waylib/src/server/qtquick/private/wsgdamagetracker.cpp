// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgdamagetracker_p.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

void WSGDamageTracker::setNodeParent(NodeId node, NodeId parent)
{
    m_nodes[node].parent = parent;
}

void WSGDamageTracker::setNodeTransform(NodeId node, const QTransform &transform)
{
    m_nodes[node].transform = transform;
}

void WSGDamageTracker::onNodeAdded(NodeId node, const QRectF &localRect)
{
    NodeData &data = m_nodes[node];
    data.localRect = localRect;
    data.lastRootRect = mapToRoot(node, localRect);
    m_frameDamage += data.lastRootRect.toAlignedRect();
}

void WSGDamageTracker::onNodeRemoved(NodeId node)
{
    auto it = m_nodes.find(node);
    if (it == m_nodes.end())
        return;
    m_frameDamage += it->lastRootRect.toAlignedRect();
    m_nodes.erase(it);
}

void WSGDamageTracker::onGeometryChanged(NodeId node, const QRectF &newLocalRect)
{
    auto it = m_nodes.find(node);
    if (it == m_nodes.end())
        return;

    NodeData &data = it.value();
    QRectF oldRootRect = data.lastRootRect;
    data.localRect = newLocalRect;
    data.lastRootRect = mapToRoot(node, newLocalRect);

    m_frameDamage += oldRootRect.toAlignedRect();
    m_frameDamage += data.lastRootRect.toAlignedRect();
}

void WSGDamageTracker::onContentDirty(NodeId node, const QRegion &localSubRegion)
{
    auto it = m_nodes.find(node);
    if (it == m_nodes.end())
        return;

    m_frameDamage += mapToRoot(node, localSubRegion);
}

void WSGDamageTracker::onTransformChanged(NodeId node, const QTransform &newTransform)
{
    auto it = m_nodes.find(node);
    if (it == m_nodes.end())
        return;

    NodeData &data = it.value();
    QRectF oldRootRect = data.lastRootRect;
    data.transform = newTransform;
    data.lastRootRect = mapToRoot(node, data.localRect);

    m_frameDamage += oldRootRect.toAlignedRect();
    m_frameDamage += data.lastRootRect.toAlignedRect();
}

QRegion WSGDamageTracker::takeFrameDamage()
{
    QRegion result;
    result.swap(m_frameDamage);
    return result;
}

void WSGDamageTracker::reset()
{
    m_frameDamage = QRegion();
    m_nodes.clear();
}

QTransform WSGDamageTracker::nodeToRootTransform(NodeId node) const
{
    QTransform t;
    NodeId current = node;
    while (current) {
        auto it = m_nodes.find(current);
        if (it == m_nodes.end())
            break;
        t = it->transform * t;
        current = it->parent;
    }
    return t;
}

QRectF WSGDamageTracker::mapToRoot(NodeId node, const QRectF &localRect) const
{
    return nodeToRootTransform(node).mapRect(localRect);
}

QRegion WSGDamageTracker::mapToRoot(NodeId node, const QRegion &localRegion) const
{
    return nodeToRootTransform(node).map(localRegion);
}

WAYLIB_SERVER_END_NAMESPACE
