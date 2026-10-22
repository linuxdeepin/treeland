// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "swipegesture.h"

#include <algorithm>
#include <cmath>

SwipeGesture::SwipeGesture(QObject *parent)
    : Gesture(parent)
{
}

bool SwipeGesture::minimumFingerCountIsRelevant() const
{
    return m_minimumFingerCountRelevant;
}

void SwipeGesture::setMinimumFingerCount(uint count)
{
    m_minimumFingerCount = count;
    m_minimumFingerCountRelevant = true;
}

uint SwipeGesture::minimumFingerCount()
{
    return m_minimumFingerCount;
}

bool SwipeGesture::maximumFingerCountIsRelevant() const
{
    return m_maximumFingerCountRelevant;
}

void SwipeGesture::setMaximumFingerCount(uint count)
{

    m_maximumFingerCount = count;
    m_maximumFingerCountRelevant = true;
}

uint SwipeGesture::maximumFingerCount()
{
    return m_maximumFingerCount;
}

SwipeGesture::Direction SwipeGesture::direction() const
{
    return m_direction;
}

void SwipeGesture::setDirection(SwipeGesture::Direction direction)
{
    m_direction = direction;
}

void SwipeGesture::setMinimumX(int x)
{
    m_minimumX = x;
    m_minimumXRelevant = true;
}

int SwipeGesture::minimumX() const
{
    return m_minimumX;
}

bool SwipeGesture::minimumXIsRelevant() const
{
    return m_minimumXRelevant;
}

void SwipeGesture::setMinimumY(int y)
{
    m_minimumY = y;
    m_minimumYRelevant = true;
}

int SwipeGesture::minimumY() const
{
    return m_minimumY;
}

bool SwipeGesture::minimumYIsRelevant() const
{
    return m_minimumYRelevant;
}

void SwipeGesture::setMaximumX(int x)
{
    m_maximumX = x;
    m_maximumXRelevant = true;
}

int SwipeGesture::maximumX() const
{
    return m_maximumX;
}

bool SwipeGesture::maximumXIsRelevant() const
{
    return m_maximumXRelevant;
}

void SwipeGesture::setMaximumY(int y)
{
    m_maximumY = y;
    m_maximumYRelevant = true;
}

int SwipeGesture::maximumY() const
{
    return m_maximumY;
}

bool SwipeGesture::maximumYIsRelevant() const
{
    return m_maximumYRelevant;
}

void SwipeGesture::setStartGeometry(const QRect &geometry)
{
    setMinimumX(geometry.x());
    setMinimumY(geometry.y());
    setMaximumX(geometry.right());
    setMaximumY(geometry.bottom());

    Q_ASSERT(m_maximumX >= m_minimumX);
    Q_ASSERT(m_maximumY >= m_minimumY);
}

QPointF SwipeGesture::minimumDelta() const
{
    return m_minimumDelta;
}

void SwipeGesture::setMinimumDelta(const QPointF &delta)
{
    m_minimumDelta = delta;
    m_minimumDeltaRelevant = true;
}

qreal SwipeGesture::deltaToProgress(const QPointF &delta) const
{
    if (!m_minimumDeltaRelevant && m_minimumDelta.isNull()) {
        return 1.0;
    }

    switch (m_direction) {
    case SwipeGesture::Up:
    case SwipeGesture::Down:
        return std::min(std::abs(delta.y()) / std::abs(m_minimumDelta.y()), 1.0);
    case SwipeGesture::Left:
    case SwipeGesture::Right:
        return std::min(std::abs(delta.x()) / std::abs(m_minimumDelta.x()), 1.0);
    default:
        Q_UNREACHABLE();
    }
}

bool SwipeGesture::minimumDeltaReached(const QPointF &delta) const
{
    return deltaToProgress(delta) >= 1.0;
}

SwipeGesture::Direction SwipeGesture::opposite(SwipeGesture::Direction direction)
{
    switch (direction) {
    case SwipeGesture::Invalid:
        return SwipeGesture::Invalid;
    case SwipeGesture::Down:
        return SwipeGesture::Up;
    case SwipeGesture::Up:
        return SwipeGesture::Down;
    case SwipeGesture::Left:
        return SwipeGesture::Right;
    case SwipeGesture::Right:
        return SwipeGesture::Left;
    }
    return SwipeGesture::Invalid;
}
