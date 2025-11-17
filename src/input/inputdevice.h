// Copyright (C) 2024-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "gestures.h"

#include <libinput.h>

#include <wglobal.h>

#include <QInputDevice>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WInputDevice;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

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

class InputDevice : public QObject
{
public:
    static InputDevice *instance();

    InputDevice(const InputDevice &) = delete;
    InputDevice &operator=(const InputDevice &) = delete;

    bool initTouchPad(WInputDevice *device);

    SwipeGesture* registerTouchpadSwipe(const SwipeFeedBack &feed_back);
    HoldGesture* registerTouchpadHold(const HoldFeedBack &feed);

    void unregisterTouchpadSwipe(SwipeGesture *gesture);
    void unregisterTouchpadHold(HoldGesture *gesture);

    void processSwipeStart(uint finger);
    void processSwipeUpdate(const QPointF &delta);
    void processSwipeCancel();
    void processSwipeEnd();

    void processHoldStart(uint finger);
    void processHoldEnd();

private:
    InputDevice(QObject *parent = nullptr);
    ~InputDevice();

    static InputDevice *m_instance;
    std::unique_ptr<GestureRecognizer> m_touchpadRecognizer;
    uint m_touchpadFingerCount = 0;
};
