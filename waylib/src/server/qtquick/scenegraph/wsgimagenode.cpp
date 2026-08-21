// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsgimagenode_p.h"

#include <QSGGeometry>

WAYLIB_SERVER_BEGIN_NAMESPACE

void WSGImageNode::setRect(const QRectF &rect)
{
    QSGDefaultImageNode::setRect(rect);
    // rebuildGeometry is a no-op without a texture; keep vertex bounds in
    // sync so the damage tracker can read the quad before setTexture.
    if (!texture())
        QSGGeometry::updateTexturedRectGeometry(geometry(), rect, QRectF(0, 0, 1, 1));
}

void WSGImageNode::setDamageRegion(const QRegion &region)
{
    if (m_explicit && m_region == region)
        return;
    m_region = region;
    m_explicit = true;
    markDirty(QSGNode::DirtyMaterial);
}

void WSGImageNode::clearDamageRegion()
{
    if (!m_explicit && m_region.isEmpty())
        return;
    m_region = QRegion();
    m_explicit = false;
    markDirty(QSGNode::DirtyMaterial);
}

WSGImageNode *WSGImageNode::enclosingNode(QSGNode *node)
{
    for (QSGNode *n = node; n; n = n->parent()) {
        if (auto *image = dynamic_cast<WSGImageNode *>(n))
            return image;
    }
    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
