// Copyright (C) 2024 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "togglablegesture.h"

#include "input/inputdevice.h"
#include "seat/helper.h"
#include "workspace/workspace.h"
#include "workspace/workspaceanimationcontroller.h"

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

void TogglableGesture::moveSlide(qreal cb)
{
    if (qFuzzyCompare(cb, m_desktopOffset))
        return;

    Workspace *workspace = Helper::instance()->workspace();
    Q_ASSERT(workspace);

    WorkspaceAnimationController *controller = workspace->animationController();
    Q_ASSERT(controller);

    m_desktopOffset = cb;
    if (!m_slideEnable) {
        m_slideEnable = true;
        m_slideBounce = false;

        m_fromId = workspace->currentIndex();
        if (cb > 0) {
            m_toId = m_fromId + 1;
            if (m_toId > workspace->count())
                return;

            if (m_toId == workspace->count())
                m_slideBounce = true;
        } else if (cb < 0) {
            m_toId = m_fromId - 1;
            if (m_toId < -1)
                return;

            if (m_toId == -1)
                m_slideBounce = true;
        }

        controller->slideNormal(m_fromId, m_toId);
        workspace->createSwitcher();
        controller->setRunning(true);
    }

    if (m_slideEnable) {
        controller->startGestureSlide(m_desktopOffset, m_slideBounce);
    }
}

void TogglableGesture::moveDischarge()
{
    if (!m_slideEnable)
        return;

    m_slideEnable = false;

    // precision control. if it is infinitely close to 0, resetting the current index will cause
    // flickering
    qreal epison = std::floor(std::abs(m_desktopOffset) * 100) / 100;
    if (epison < 0.01)
        return;

    Workspace *workspace = Helper::instance()->workspace();
    if (!m_slideBounce && (m_desktopOffset > 0.98 || m_desktopOffset < -0.98)) {
        // m_desktopOffset is very close to 1 or -1, just set to the toId directly
        // Not need to play the slide animation
        workspace->setCurrentIndex(m_toId);
        auto *controller = workspace->animationController();
        controller->setRunning(false);
        return;
    }

    m_fromId = workspace->currentIndex();
    m_toId = 0;

    if (m_desktopOffset > 0.3) {
        m_toId = m_slideBounce ? m_fromId : m_fromId + 1;
        if (m_toId >= workspace->count())
            return;
    } else if (m_desktopOffset <= -0.3) {
        m_toId = m_slideBounce ? m_fromId : m_fromId - 1;
        if (m_toId < 0)
            return;
    } else {
        m_toId = m_fromId;
    }

    auto controller = workspace->animationController();
    if (m_toId >= 0 && m_toId < workspace->count()) {
        controller->slideRunning(m_toId);
        controller->startSlideAnimation();
        workspace->setCurrentIndex(m_toId);
    }
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
            moveSlide(cb);
        };

        const auto right = [this](qreal cb) {
            moveSlide(-cb);
        };

        const auto trigger = [this]() mutable {
            moveDischarge();
        };

        InputDevice::instance()->registerTouchpadSwipe(
            SwipeFeedBack{ SwipeGesture::Left, finger, trigger, left });

        InputDevice::instance()->registerTouchpadSwipe(
            SwipeFeedBack{ SwipeGesture::Right, finger, trigger, right });
    }
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
