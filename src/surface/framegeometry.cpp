// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "framegeometry.h"

#include <algorithm>
#include <cmath>

FrameGeometry::FrameGeometry(const QRectF &rect, qreal radius)
{
    if (rect.isEmpty() || !std::isfinite(rect.left()) || !std::isfinite(rect.top())
        || !std::isfinite(rect.right()) || !std::isfinite(rect.bottom()))
        return;
    m_rect = rect;
    if (std::isfinite(radius))
        m_radius = std::clamp(radius, qreal(0), std::min(rect.width(), rect.height()) / 2);
}

bool FrameGeometry::contains(const QPointF &point) const
{
    // Match Qt Quick's half-open bounds, including when used as a containmentMask.
    if (!(point.x() >= m_rect.left() && point.x() < m_rect.right()
          && point.y() >= m_rect.top() && point.y() < m_rect.bottom()))
        return false;
    if (m_radius == 0)
        return true;
    const qreal dx = point.x() - std::clamp(point.x(), m_rect.left() + m_radius,
                                          m_rect.right() - m_radius);
    const qreal dy = point.y() - std::clamp(point.y(), m_rect.top() + m_radius,
                                          m_rect.bottom() - m_radius);
    return dx * dx + dy * dy <= m_radius * m_radius;
}

Qt::Edges FrameGeometry::resizeEdgesAt(const QPointF &point, qreal margin) const
{
    if (m_rect.isEmpty() || !std::isfinite(margin) || margin <= 0 || contains(point))
        return {};
    // Keep square resize corners for square windows.
    const FrameGeometry outer(m_rect.adjusted(-margin, -margin, margin, margin),
                              m_radius > 0 ? m_radius + margin : 0);
    if (!outer.contains(point))
        return {};
    Qt::Edges edges;
    if (point.x() < m_rect.left() + m_radius)
        edges |= Qt::LeftEdge;
    else if (point.x() >= m_rect.right() - m_radius)
        edges |= Qt::RightEdge;
    if (point.y() < m_rect.top() + m_radius)
        edges |= Qt::TopEdge;
    else if (point.y() >= m_rect.bottom() - m_radius)
        edges |= Qt::BottomEdge;
    return edges;
}
