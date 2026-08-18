// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QHash>
#include <QRegion>
#include <QSGNode>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSGImageNode;

class WAYLIB_SERVER_EXPORT WSGDamageTracker
{
public:
    void nodeChanged(QSGNode *node, QSGNode::DirtyState state);
    void markFull();
    void commit();
    void reset();

    QRegion flushRegion() const { return m_flushRegion; }
    bool flushRegionIsFull() const { return m_flushRegionIsFull; }
    QRegion pendingRegion() const { return m_pendingDamage; }
    bool pendingIsFull() const { return m_fullDamage; }
    void addRegion(const QRegion &region);
    void addToFlush(const QRegion &region);

    // Per-node damage recorded this frame (old∪new).
    QRegion damageForNode(const QSGNode *node) const;
    QRegion removedDamage() const { return m_removedDamage; }
    void clearNodeDamage();

private:
    QMatrix4x4 combinedMatrix(const QSGNode *node) const;
    QRectF itemBounds(QSGNode *node) const;
    QRegion subtreeItemBounds(QSGNode *node) const;
    QRegion liveBounds(QSGNode *node) const;
    QRegion mappedDamageRegion(WSGImageNode *node) const;
    void addRect(const QRect &rect);
    void dropCachedBounds(QSGNode *node);
    void cacheSubtreeBounds(QSGNode *node);
    void recordNodeDamage(QSGNode *node, const QRegion &region);

    QRegion m_pendingDamage;
    QRegion m_flushRegion;
    QHash<QSGNode *, QRect> m_lastItemBounds;
    QHash<QSGNode *, QRegion> m_nodeDamage;
    QRegion m_removedDamage;
    bool m_fullDamage = false;
    bool m_flushRegionIsFull = false;
};

// True when `flush` has damage in `mapped` that is not just the blitter's
// own full quad (textureChanged after the last copy). A proper subset is
// content behind the glass (waylib blur example) and must recopy; treating
// every contained rect as a caret froze the frosted-glass texture.
inline bool wsgFlushRequiresCapture(const QRegion &flush, bool full,
                                    const QRect &lastCapture, const QRect &mapped)
{
    if (lastCapture.isNull() || full)
        return true;
    for (const QRect &rect : flush) {
        if (!rect.intersects(mapped))
            continue;
        const QRect paddedCapture = lastCapture.adjusted(-1, -1, 1, 1);
        const QRect paddedRect = rect.adjusted(-1, -1, 1, 1);
        if (paddedCapture.contains(rect) && paddedRect.contains(lastCapture))
            continue;
        return true;
    }
    return false;
}

WAYLIB_SERVER_END_NAMESPACE
