
// Copyright (C) 2024 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QPointF>

class Gesture : public QObject
{
    Q_OBJECT
public:
    Gesture(QObject *parent = nullptr);

Q_SIGNALS:
    void started();
    void triggered();
    void cancelled();
};

class SwipeGesture : public Gesture
{
    Q_OBJECT
public:
    explicit SwipeGesture(QObject *parent = nullptr);

    enum Direction
    {
        Invalid,
        Down,
        Left,
        Up,
        Right,
    };

    bool minimumFingerCountIsRelevant() const;
    void setMinimumFingerCount(uint count);
    uint minimumFingerCount();

    bool maximumFingerCountIsRelevant() const;
    void setMaximumFingerCount(uint count);
    uint maximumFingerCount();

    Direction direction() const;
    void setDirection(Direction direction);

    void setMinimumX(int x);
    int minimumX() const;
    bool minimumXIsRelevant() const;
    void setMinimumY(int y);
    int minimumY() const;
    bool minimumYIsRelevant() const;

    void setMaximumX(int x);
    int maximumX() const;
    bool maximumXIsRelevant() const;
    void setMaximumY(int y);
    int maximumY() const;
    bool maximumYIsRelevant() const;
    void setStartGeometry(const QRect &geometry);

    QPointF minimumDelta() const;
    void setMinimumDelta(const QPointF &delta);

    qreal deltaToProgress(const QPointF &delta) const;
    bool minimumDeltaReached(const QPointF &delta) const;

Q_SIGNALS:
    void progress(qreal);
    void deltaProgress(const QPointF &delta);

private:
    bool m_minimumFingerCountRelevant = false;
    uint m_minimumFingerCount = 0;
    bool m_maximumFingerCountRelevant = false;
    uint m_maximumFingerCount = 0;
    SwipeGesture::Direction m_direction = SwipeGesture::Down;
    bool m_minimumXRelevant = false;
    int m_minimumX = 0;
    bool m_minimumYRelevant = false;
    int m_minimumY = 0;
    bool m_maximumXRelevant = false;
    int m_maximumX = 0;
    bool m_maximumYRelevant = false;
    int m_maximumY = 0;
    bool m_minimumDeltaRelevant = false;
    QPointF m_minimumDelta;
};

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

    int startSwipeGesture(uint fingerCount);
    int startSwipeGesture(const QPointF &startPos);

    void updateSwipeGesture(const QPointF &delta);
    void cancelSwipeGesture();
    void endSwipeGesture();

private:
    void cancelActiveGestures();
    int startSwipeGesture(uint fingerCount,
                          const QPointF &start_pos,
                          StartPositionBehavior behavior);

    QList<SwipeGesture *> m_swipeGestures;
    QList<SwipeGesture *> m_activeSwipeGestures;
    QMap<Gesture *, QMetaObject::Connection> m_destroyConnections;

    QPointF m_currentDelta = QPointF(0, 0);
    uint m_currentFingerCount = 0;
    GestureRecognizer::Axis m_currentSwipeAxis = GestureRecognizer::None;
};
