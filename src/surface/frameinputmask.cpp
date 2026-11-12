// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "frameinputmask.h"

FrameInputMask::FrameInputMask(QQuickItem *parent)
    : QQuickItem(parent)
{
}

void FrameInputMask::setFrameItem(QQuickItem *item)
{
    if (m_frameItem == item)
        return;
    m_frameItem = item;
    Q_EMIT frameItemChanged();
}

void FrameInputMask::setTarget(QQuickItem *item)
{
    if (m_target == item)
        return;
    m_target = item;
    Q_EMIT targetChanged();
}

bool FrameInputMask::inTarget(const QPointF &point) const
{
    return m_frameItem && m_target
        && point.x() >= 0 && point.y() >= 0
        && point.x() < m_target->width() && point.y() < m_target->height();
}

bool FrameInputMask::contains(const QPointF &point) const
{
    if (m_resizeMargin > 0)
        return edgesAt(point) != Qt::Edges();
    return inTarget(point) && m_geometry.contains(m_target->mapToItem(m_frameItem, point));
}

Qt::Edges FrameInputMask::edgesAt(const QPointF &point) const
{
    if (!inTarget(point))
        return {};
    return m_geometry.resizeEdgesAt(m_target->mapToItem(m_frameItem, point), m_resizeMargin);
}
