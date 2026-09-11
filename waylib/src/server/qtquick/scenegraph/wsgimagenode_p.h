// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <private/qsgdefaultimagenode_p.h>

#include <wglobal.h>
#include <wpixmanregion.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

// QSGDefaultImageNode plus optional content damage. WSGContext::createImageNode
// returns this so Wayland surfaces can report buffer damage on the node
// itself instead of wrapping it in an extra QSGNode.
class WAYLIB_SERVER_EXPORT WSGImageNode : public QSGDefaultImageNode
{
public:
    void setRect(const QRectF &rect) override;

    void setDamageRegion(const WPixmanRegion &region);
    void clearDamageRegion();

    bool hasExplicitDamage() const
    {
        return m_explicit;
    }

    const WPixmanRegion &damageRegion() const
    {
        return m_region;
    }

    // Surface-local opaque mapped into item coordinates (same space as
    // damageRegion). Empty + hasExplicitOpaque means the client claimed
    // nothing opaque; do not fall back to material blending.
    void setOpaqueRegion(const WPixmanRegion &region);

    bool hasExplicitOpaque() const
    {
        return m_opaqueExplicit;
    }

    const WPixmanRegion &opaqueRegion() const
    {
        return m_opaque;
    }

    static WSGImageNode *enclosingNode(QSGNode *node);

private:
    WPixmanRegion m_region;
    WPixmanRegion m_opaque;
    bool m_explicit = false;
    bool m_opaqueExplicit = false;
};

WAYLIB_SERVER_END_NAMESPACE
