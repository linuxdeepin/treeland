// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "gesturemanager.h"
#include "gesturerecognizer.h"

#define MIN_SWIPE_FINGERS 3

GestureManager *GestureManager::m_instance = nullptr;

GestureManager::GestureManager(QObject *parent)
    : QObject(parent)
    , m_touchpadRecognizer(new GestureRecognizer(this))
{
}

GestureManager::~GestureManager()
{
    if (m_instance == this)
        m_instance = nullptr;
}

GestureManager *GestureManager::instance()
{
    if (m_instance == nullptr) {
        m_instance = new GestureManager();
    }

    return m_instance;
}

[[maybe_unused]] SwipeGesture * GestureManager::registerTouchpadSwipe(const SwipeFeedBack &feed_back)
{
    auto swipe_gesture = new SwipeGesture();
    swipe_gesture->setDirection(feed_back.direction);
    swipe_gesture->setMinimumDelta(QPointF(200, 200));
    swipe_gesture->setMaximumFingerCount(feed_back.fingerCount);
    swipe_gesture->setMinimumFingerCount(feed_back.fingerCount);

    if (feed_back.actionCallback) {
        QObject::connect(swipe_gesture, &SwipeGesture::triggered, [fb = feed_back.actionCallback]() {
            fb(true);
        });
        QObject::connect(swipe_gesture, &SwipeGesture::cancelled, [fb = feed_back.actionCallback]() {
            fb(false);
        });
    }

    if (feed_back.progressCallback) {
        QObject::connect(swipe_gesture, &SwipeGesture::progress, feed_back.progressCallback);
    }

    m_touchpadRecognizer->registerSwipeGesture(swipe_gesture);
    return swipe_gesture;
}

[[maybe_unused]] HoldGesture * GestureManager::registerTouchpadHold(const HoldFeedBack &feed)
{
    auto hold_gesture = new HoldGesture();
    hold_gesture->setMinimumFingerCount(feed.fingerCount);
    hold_gesture->setMaximumFingerCount(feed.fingerCount);

    if (feed.actionCallback) {
        QObject::connect(hold_gesture, &HoldGesture::cancelled, feed.actionCallback);
    }

    if (feed.longProcessCallback) {
        QObject::connect(hold_gesture, &HoldGesture::longPressed, feed.longProcessCallback);
    }

    m_touchpadRecognizer->registerHoldGesture(hold_gesture);
    return hold_gesture;
}

void GestureManager::unregisterTouchpadSwipe(SwipeGesture *gesture)
{
    m_touchpadRecognizer->unregisterSwipeGesture(gesture);
}

void GestureManager::unregisterTouchpadHold(HoldGesture *gesture)
{
    m_touchpadRecognizer->unregisterHoldGesture(gesture);
}

void GestureManager::processSwipeStart(uint finger)
{
    m_touchpadFingerCount = finger;
    if (m_touchpadFingerCount >= MIN_SWIPE_FINGERS) {
        m_touchpadRecognizer->startSwipeGesture(finger);
    }
}

void GestureManager::processSwipeUpdate(const QPointF &delta)
{
    if (m_touchpadFingerCount >= MIN_SWIPE_FINGERS) {
        m_touchpadRecognizer->updateSwipeGesture(delta);
    }
}

void GestureManager::processSwipeCancel()
{
    if (m_touchpadFingerCount >= MIN_SWIPE_FINGERS) {
        m_touchpadRecognizer->cancelSwipeGesture();
    }
}

void GestureManager::processSwipeEnd()
{
    if (m_touchpadFingerCount >= MIN_SWIPE_FINGERS) {
        m_touchpadRecognizer->endSwipeGesture();
    }
}

void GestureManager::processHoldStart(uint finger)
{
    m_touchpadFingerCount = finger;
    if (m_touchpadFingerCount >= MIN_SWIPE_FINGERS) {
        m_touchpadRecognizer->startHoldGesture(finger);
    }
}

void GestureManager::processHoldEnd()
{
    if (m_touchpadFingerCount >= MIN_SWIPE_FINGERS) {
        m_touchpadRecognizer->endHoldGesture();
    }
}

void GestureManager::setHoldTimeout(int msec)
{
    m_touchpadRecognizer->setHoldTimeout(msec);
}
