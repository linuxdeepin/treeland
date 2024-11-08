// Copyright (C) 2024 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "gestures.h"

#include <QLoggingCategory>
#include <QRect>

// The minimum delta required to recognize a swipe gesture
#define SWIPE_MINIMUM_DELTA 5

Q_DECLARE_LOGGING_CATEGORY(gestures);
Q_LOGGING_CATEGORY(gestures, "treeland.gestures", QtDebugMsg);

Gesture::Gesture(QObject *parent)
    : QObject(parent)
{
}

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

GestureRecognizer::GestureRecognizer(QObject *parent)
    : QObject(parent)
{
}

void GestureRecognizer::registerSwipeGesture(SwipeGesture *gesture)
{
    Q_ASSERT(!m_swipeGestures.contains(gesture));
    auto connection = connect(gesture,
                              &QObject::destroyed,
                              this,
                              std::bind(&GestureRecognizer::unregisterSwipeGesture, this, gesture));
    m_destroyConnections.insert(gesture, connection);
    m_swipeGestures << gesture;
}

void GestureRecognizer::unregisterSwipeGesture(SwipeGesture *gesture)
{
    auto it = m_destroyConnections.find(gesture);
    if (it != m_destroyConnections.end()) {
        disconnect(it.value());
        m_destroyConnections.erase(it);
    }
    m_swipeGestures.removeAll(gesture);
    if (m_activeSwipeGestures.removeOne(gesture)) {
        Q_EMIT gesture->cancelled();
    }
    gesture->deleteLater();
}

int GestureRecognizer::startSwipeGesture(uint fingerCount)
{
    return startSwipeGesture(fingerCount, QPointF(), GestureRecognizer::Irrelevant);
}

int GestureRecognizer::startSwipeGesture(const QPointF &startPos)
{
    return startSwipeGesture(1, startPos, GestureRecognizer::Relevant);
}

void GestureRecognizer::updateSwipeGesture(const QPointF &delta)
{
    m_currentDelta += delta;

    SwipeGesture::Direction direction;
    Axis swipeAxis;

    if (m_currentSwipeAxis == Axis::None) {
        if (std::abs(m_currentDelta.x()) >= std::abs(m_currentDelta.y())) {
            swipeAxis = Axis::Horizontal;
            direction = m_currentDelta.x() < 0 ? SwipeGesture::Left : SwipeGesture::Right;
        } else {
            swipeAxis = Axis::Vertical;
            direction = m_currentDelta.y() < 0 ? SwipeGesture::Up : SwipeGesture::Down;
        }
        if (std::abs(m_currentDelta.x()) >= SWIPE_MINIMUM_DELTA
            || std::abs(m_currentDelta.y()) >= SWIPE_MINIMUM_DELTA) {
            m_currentSwipeAxis = swipeAxis;
        }
    } else {
        swipeAxis = m_currentSwipeAxis;
    }

    switch (swipeAxis) {
    case Axis::Vertical:
        direction = m_currentDelta.y() < 0 ? SwipeGesture::Up : SwipeGesture::Down;
        break;
    case Axis::Horizontal:
        direction = m_currentDelta.x() < 0 ? SwipeGesture::Left : SwipeGesture::Right;
        break;
    default:
        qCWarning(gestures) << "Invalid swipe axis";
        return;
    }

    // Eliminating wrong gestures requires two iterations
    for (int i = 0; i < 2; i++) {
        if (m_activeSwipeGestures.isEmpty()) {
            startSwipeGesture(m_currentFingerCount);
        }

        m_activeSwipeGestures.erase(
            std::remove_if(m_activeSwipeGestures.begin(),
                           m_activeSwipeGestures.end(),
                           [direction](SwipeGesture *g) {
                               if (g->direction() != direction) {
                                   if (!g->minimumXIsRelevant() || !g->maximumXIsRelevant()
                                       || !g->minimumYIsRelevant() || !g->maximumYIsRelevant()) {
                                       Q_EMIT g->cancelled();
                                       return true;
                                   }
                               }
                               return false;
                           }),
            m_activeSwipeGestures.end());
    }

    for (auto &&gesture : std::as_const(m_activeSwipeGestures)) {
        Q_EMIT gesture->progress(gesture->deltaToProgress(m_currentDelta));
        Q_EMIT gesture->deltaProgress(m_currentDelta);
    }
}

void GestureRecognizer::cancelSwipeGesture()
{
    cancelSwipeActiveGestures();
    m_currentFingerCount = 0;
    m_currentDelta = QPointF(0, 0);
    m_currentSwipeAxis = Axis::None;
}

void GestureRecognizer::endSwipeGesture()
{
    const QPointF delta = m_currentDelta;
    for (auto &&gesture : std::as_const(m_activeSwipeGestures)) {
        if (gesture->minimumDeltaReached(delta)) {
            Q_EMIT gesture->triggered();
        } else {
            Q_EMIT gesture->cancelled();
        }
    }
    m_activeSwipeGestures.clear();
    m_currentFingerCount = 0;
    m_currentDelta = QPointF(0, 0);
    m_currentSwipeAxis = Axis::None;
}

void GestureRecognizer::cancelSwipeActiveGestures()
{
    for (auto &&gesture : std::as_const(m_activeSwipeGestures)) {
        Q_EMIT gesture->cancelled();
    }
    m_activeSwipeGestures.clear();
    m_currentDelta = QPointF(0, 0);
    m_currentSwipeAxis = Axis::None;
}

int GestureRecognizer::startSwipeGesture(uint fingerCount,
                                         const QPointF &start_pos,
                                         StartPositionBehavior behavior)
{
    m_currentFingerCount = fingerCount;
    if (!m_activeSwipeGestures.isEmpty()) {
        return 0;
    }
    int count = 0;
    for (auto &&gesture : std::as_const(m_swipeGestures)) {
        if ((gesture->minimumFingerCountIsRelevant() && gesture->minimumFingerCount() > fingerCount)
            || (gesture->maximumFingerCountIsRelevant()
                && gesture->maximumFingerCount() < fingerCount)) {
            continue;
        }
        if (behavior == StartPositionBehavior::Relevant) {
            if ((gesture->minimumXIsRelevant() && gesture->minimumX() > start_pos.x())
                || (gesture->maximumXIsRelevant() && gesture->maximumX() < start_pos.x())
                || (gesture->minimumYIsRelevant() && gesture->minimumY() > start_pos.y())
                || (gesture->maximumYIsRelevant() && gesture->maximumY() < start_pos.y())) {
                continue;
            }
        }
        switch (gesture->direction()) {
        case SwipeGesture::Up:
            [[fallthrough]];
        case SwipeGesture::Down:
            if (m_currentSwipeAxis == Axis::Horizontal) {
                continue;
            }
            break;
        case SwipeGesture::Left:
            [[fallthrough]];
        case SwipeGesture::Right:
            if (m_currentSwipeAxis == Axis::Vertical) {
                continue;
            }
            break;
        case SwipeGesture::Invalid:
            qCWarning(gestures) << "Invalid swipe direction";
            continue;
        }

        m_activeSwipeGestures << gesture;
        count++;
        Q_EMIT gesture->started();
    }
    return count;
}

HoldGesture::HoldGesture(QObject *parent)
    : Gesture(parent)
    , m_holdTimer(new QTimer(this))
{
    m_holdTimer->setSingleShot(true);
    m_holdTimer->setInterval(1000);
    connect(m_holdTimer, &QTimer::timeout, this, &HoldGesture::longPressed);
}

HoldGesture::~HoldGesture()
{
    if (m_holdTimer != nullptr) {
        m_holdTimer->stop();
        m_holdTimer->deleteLater();
    }
}

void HoldGesture::startTimer()
{
    m_holdTimer->start();
}

void HoldGesture::setInterval(int msec)
{
    m_holdTimer->setInterval(msec);
}

void HoldGesture::stopTimer()
{
    m_holdTimer->stop();
}

bool HoldGesture::isActive() const
{
    return m_holdTimer->isActive();
}

void GestureRecognizer::registerHoldGesture(HoldGesture *gesture)
{
    Q_ASSERT(!m_holdGestures.contains(gesture));
    auto connection = connect(gesture,
                              &QObject::destroyed,
                              this,
                              std::bind(&GestureRecognizer::unregisterHoldGesture, this, gesture));
    m_destroyConnections.insert(gesture, connection);
    m_holdGestures << gesture;
}

void GestureRecognizer::unregisterHoldGesture(HoldGesture *gesture)
{
    auto it = m_destroyConnections.find(gesture);
    if (it != m_destroyConnections.end()) {
        disconnect(it.value());
        m_destroyConnections.erase(it);
    }
    if (m_holdGestures.removeOne(gesture)) {
        Q_EMIT gesture->cancelled();
    }
    gesture->deleteLater();
}

void GestureRecognizer::startHoldGesture(uint fingerCount)
{
    m_currentFingerCount = fingerCount;
    for (auto &&gesture : std::as_const(m_holdGestures)) {
        if (!gesture->isActive()) {
            gesture->startTimer();
            m_activeHoldGestures << gesture;
        }
    }
}

void GestureRecognizer::endHoldGesture()
{
    for (auto &&gesture : std::as_const(m_activeHoldGestures)) {
        if (gesture->isActive()) {
            gesture->stopTimer();
        }
        Q_EMIT gesture->cancelled();
    }
    m_activeHoldGestures.clear();
    m_currentFingerCount = 0;
}
