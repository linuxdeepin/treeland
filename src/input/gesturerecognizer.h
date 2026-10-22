// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "swipegesture.h"
#include "holdgesture.h"

#include <QList>
#include <QObject>
#include <QPointF>

class GestureRecognizer : public QObject
{
    Q_OBJECT
public:
    explicit GestureRecognizer(QObject *parent = nullptr);

    enum StartPositionBehavior
    {
        Relevant,
        Irrelevant,
    };

    enum Axis
    {
        Horizontal,
        Vertical,
        None,
    };

    Q_ENUM(StartPositionBehavior)
    Q_ENUM(Axis)

    void registerSwipeGesture(SwipeGesture *gesture);
    void unregisterSwipeGesture(SwipeGesture *gesture);

    void registerHoldGesture(HoldGesture *gesture);
    void unregisterHoldGesture(HoldGesture *gesture);

    int startSwipeGesture(uint fingerCount);
    int startSwipeGesture(const QPointF &startPos);

    void updateSwipeGesture(const QPointF &delta);
    void cancelSwipeGesture();
    void endSwipeGesture();

    void startHoldGesture(uint fingerCount);
    void endHoldGesture();
    void setHoldTimeout(int msec);

private:
    void cancelSwipeActiveGestures();
    int startSwipeGesture(uint fingerCount,
                          const QPointF &start_pos,
                          StartPositionBehavior behavior);

    QList<SwipeGesture *> m_swipeGestures;
    QList<SwipeGesture *> m_activeSwipeGestures;
    QList<HoldGesture *> m_holdGestures;
    QList<HoldGesture *> m_activeHoldGestures;

    QPointF m_currentDelta = QPointF(0, 0);
    uint m_currentFingerCount = 0;
    int m_holdTimeout = 0;
    GestureRecognizer::Axis m_currentSwipeAxis = GestureRecognizer::None;
};
