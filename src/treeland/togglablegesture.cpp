// Copyright (C) 2024 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "togglablegesture.h"
#include "inputdevice.h"

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

void TogglableGesture::activate()
{
    setInProgress(false);
    setPartialGestureFactor(1.0);
    setStatus(Status::Active);
}

void TogglableGesture::deactivate()
{
    setInProgress(false);
    setPartialGestureFactor(0.0);
    setStatus(Status::Inactive);
}

void TogglableGesture::toggle()
{
    if (m_status != Status::Active) {
        activate();
        Q_EMIT activated();
    } else {
        deactivate();
        Q_EMIT deactivated();
    }
}

void TogglableGesture::stop()
{
    setInProgress(false);
    setPartialGestureFactor(0.0);
    setStatus(Status::Stopped);
}

void TogglableGesture::setStatus(Status status)
{
    if (m_status != status) {
        m_status = status;
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

std::function<void(void)> TogglableGesture::activeTriggeredCallback()
{
    return [this]() {
        activeTriggered();
    };
}

std::function<void(void)> TogglableGesture::deactivateTriggeredCallback()
{
    return [this]() {
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

static SwipeGesture::Direction opposite(SwipeGesture::Direction direction)
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

void TogglableGesture::setDesktopOffset(qreal offset)
{
    m_desktopOffset = offset;
    Q_EMIT desktopOffsetChanged();
}

void TogglableGesture::addTouchpadSwipeGesture(SwipeGesture::Direction direction, uint finger)
{
    if (direction == SwipeGesture::Invalid)
        return;

    if (direction == SwipeGesture::Up || direction == SwipeGesture::Down) {
        InputDevice::instance()->registerTouchpadSwipe(
            SwipeFeedBack{ direction,
                           finger,
                           this->activeTriggeredCallback(),
                           this->progressCallback() });

        InputDevice::instance()->registerTouchpadSwipe(
            SwipeFeedBack{ opposite(direction),
                           finger,
                           this->deactivateTriggeredCallback(),
                           this->regressCallback() });
    } else {
        const auto left = [this](qreal cb) {
            m_desktopOffsetRelevant = true;

            if (desktopOffset() != cb)
                setDesktopOffset(cb);
        };

        const auto right = [this](qreal cb) {
            m_desktopOffsetRelevant = true;
            if (desktopOffset() != cb)
                setDesktopOffset(-cb);
        };

        const auto trigger = [this]() mutable {
            if (m_desktopOffsetRelevant) {
                m_desktopOffsetRelevant = false;
                Q_EMIT desktopOffsetCancelled();
            }
        };

        InputDevice::instance()->registerTouchpadSwipe(
            SwipeFeedBack{ SwipeGesture::Left, finger, trigger, left });

        InputDevice::instance()->registerTouchpadSwipe(
            SwipeFeedBack{ SwipeGesture::Right, finger, trigger, right });
    }
}

