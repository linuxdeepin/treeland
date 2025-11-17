// Copyright (C) 2024-2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "togglablegesture.h"

#include "input/inputdevice.h"

TogglableGesture::TogglableGesture(QObject *parent)
    : QObject(parent)
{
}

TogglableGesture::~TogglableGesture() { }

bool TogglableGesture::inProgress() const
{
    return m_inProgress;
}

void TogglableGesture::setInProgress(bool gesture)
{
    if (m_inProgress != gesture) {
        m_inProgress = gesture;
        Q_EMIT inProgressChanged();
    }
}

void TogglableGesture::setPartialGestureFactor(qreal factor)
{
    if (m_partialGestureFactor != factor) {
        m_partialGestureFactor = factor;
        Q_EMIT partialGestureFactorChanged(factor);
    }
}

void TogglableGesture::activate(bool on)
{
    setInProgress(false);
    setPartialGestureFactor(1.0);
    setStatus(Status::Active, on);
}

void TogglableGesture::deactivate(bool on)
{
    setInProgress(false);
    setPartialGestureFactor(0.0);
    setStatus(Status::Inactive, on);
}

void TogglableGesture::toggle(bool on)
{
    if (m_status != Status::Active) {
        activate(on);
        if (on)
            Q_EMIT activated();
    } else {
        deactivate(on);
        if (on)
            Q_EMIT deactivated();
    }
}

void TogglableGesture::stop()
{
    setInProgress(false);
    setPartialGestureFactor(0.0);
    setStatus(Status::Stopped);
}

void TogglableGesture::setStatus(Status status, bool on)
{
    if (m_status != status) {
        m_status = status;
        if (on)
            Q_EMIT statusChanged(status);
    }
}

std::function<void(qreal progress)> TogglableGesture::progressCallback()
{
    return [this](qreal progress) {
        setProgress(progress);
    };
}

std::function<void(qreal progress)> TogglableGesture::regressCallback()
{
    return [this](qreal progress) {
        setRegress(progress);
    };
}

void TogglableGesture::setProgress(qreal progress)
{
    if (m_status == Status::Stopped) {
        return;
    }

    switch (m_status) {
    case Status::Inactive:
    case Status::Activating: {
        setStatus(Status::Activating);
        setInProgress(true);
        setPartialGestureFactor(progress);
        break;
    }
    default:
        break;
    }
}

void TogglableGesture::setRegress(qreal regress)
{
    if (m_status == Status::Stopped) {
        return;
    }

    switch (m_status) {
    case Status::Active:
    case Status::Deactivating:
        setStatus(Status::Deactivating);
        setInProgress(true);
        setPartialGestureFactor(1.0 - regress);
        break;
    default:
        break;
    }
}

std::function<void(bool)> TogglableGesture::activeTriggeredCallback()
{
    return [this](bool) {
        activeTriggered();
    };
}

std::function<void(bool)> TogglableGesture::deactivateTriggeredCallback()
{
    return [this](bool) {
        deactivateTriggered();
    };
}

void TogglableGesture::activeTriggered()
{
    if (m_status == Status::Activating) {
        if (m_partialGestureFactor > 0.5) {
            activate();
            Q_EMIT activated();
        } else {
            deactivate();
            Q_EMIT deactivated();
        }
    }
}

void TogglableGesture::deactivateTriggered()
{
    if (m_status == Status::Deactivating) {
        if (m_partialGestureFactor < 0.5) {
            deactivate();
            Q_EMIT deactivated();
        } else {
            activate();
            Q_EMIT activated();
        }
    }
}

void TogglableGesture::addTouchpadSwipeGesture(SwipeGesture::Direction direction, uint finger)
{
    if (direction == SwipeGesture::Invalid)
        return;

    InputDevice::instance()->registerTouchpadSwipe(
        SwipeFeedBack{ direction,
                       finger,
                       this->activeTriggeredCallback(),
                       this->progressCallback() });

    InputDevice::instance()->registerTouchpadSwipe(
        SwipeFeedBack{ SwipeGesture::opposite(direction),
                       finger,
                       this->deactivateTriggeredCallback(),
                       this->regressCallback() });
}

void TogglableGesture::addTouchpadHoldGesture(uint finger)
{
    const auto pressed = [this]() {
        Q_EMIT longPressed();
    };

    const auto trigger = [this]() {
        Q_EMIT hold();
    };

    InputDevice::instance()->registerTouchpadHold(HoldFeedBack{ finger, trigger, pressed });
}
