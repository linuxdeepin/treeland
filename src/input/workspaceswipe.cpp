// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "workspaceswipe.h"

#include "input/inputdevice.h"
#include "seat/helper.h"
#include "workspace/workspace.h"
#include "workspace/workspaceanimationcontroller.h"

WorkspaceSwipeGesture::WorkspaceSwipeGesture(SwipeGesture::Direction direction, uint finger, QObject *parent)
    : QObject(parent)
{
    Q_ASSERT(direction != SwipeGesture::Direction::Invalid);

    const auto forward = [this](qreal cb) {
        moveSlide(cb);
    };

    const auto backword = [this](qreal cb) {
        moveSlide(-cb);
    };

    const auto trigger = [this]() mutable {
        moveDischarge();
    };

    forwardGesture = InputDevice::instance()->registerTouchpadSwipe(
        SwipeFeedBack{ direction, finger, trigger, forward });

    backwordGesture = InputDevice::instance()->registerTouchpadSwipe(
        SwipeFeedBack{ SwipeGesture::opposite(direction), finger, trigger, backword });
}

void WorkspaceSwipeGesture::destroy()
{
    if (forwardGesture) {
        InputDevice::instance()->unregisterTouchpadSwipe(forwardGesture);
        forwardGesture = nullptr;
    }

    if (backwordGesture) {
        InputDevice::instance()->unregisterTouchpadSwipe(backwordGesture);
        backwordGesture = nullptr;
    }
}

WorkspaceSwipeGesture::~WorkspaceSwipeGesture()
{
    destroy();
}

void WorkspaceSwipeGesture::moveSlide(qreal cb)
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

void WorkspaceSwipeGesture::moveDischarge()
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




