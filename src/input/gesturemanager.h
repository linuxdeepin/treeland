// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "gestures.h"

#include <functional>
#include <memory>

struct SwipeFeedBack
{
    SwipeGesture::Direction direction;
    uint fingerCount;
    std::function<void(bool)> actionCallback;
    std::function<void(qreal)> progressCallback;
};

struct HoldFeedBack
{
    uint fingerCount;
    std::function<void()> actionCallback;
    std::function<void()> longProcessCallback;
};

class GestureManager : public QObject
{
public:
    static GestureManager *instance();

    GestureManager(const GestureManager &) = delete;
    GestureManager &operator=(const GestureManager &) = delete;

    SwipeGesture *registerTouchpadSwipe(const SwipeFeedBack &feed_back);
    HoldGesture *registerTouchpadHold(const HoldFeedBack &feed);

    void unregisterTouchpadSwipe(SwipeGesture *gesture);
    void unregisterTouchpadHold(HoldGesture *gesture);

    void processSwipeStart(uint finger);
    void processSwipeUpdate(const QPointF &delta);
    void processSwipeCancel();
    void processSwipeEnd();

    void processHoldStart(uint finger);
    void processHoldEnd();
    void setHoldTimeout(int msec);

private:
    GestureManager(QObject *parent = nullptr);
    ~GestureManager();

    static GestureManager *m_instance;
    std::unique_ptr<GestureRecognizer> m_touchpadRecognizer;
    uint m_touchpadFingerCount = 0;
};

