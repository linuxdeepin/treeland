// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <private/qsgdefaultimagenode_p.h>

#include <QRegion>

WAYLIB_SERVER_BEGIN_NAMESPACE

// QSGDefaultImageNode plus optional content damage. WSGContext::createImageNode
// returns this so Wayland surfaces can report buffer damage on the node
// itself instead of wrapping it in an extra QSGNode.
class WAYLIB_SERVER_EXPORT WSGImageNode : public QSGDefaultImageNode
{
public:
    void setRect(const QRectF &rect) override;

    void setDamageRegion(const QRegion &region);
    void clearDamageRegion();

    bool hasExplicitDamage() const { return m_explicit; }
    QRegion damageRegion() const { return m_region; }

    static WSGImageNode *enclosingNode(QSGNode *node);

private:
    QRegion m_region;
    bool m_explicit = false;
};

WAYLIB_SERVER_END_NAMESPACE
