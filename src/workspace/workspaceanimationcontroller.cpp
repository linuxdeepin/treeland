// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "workspaceanimationcontroller.h"

#include "plugins/multitaskview/multitaskview.h"

#include <cmath>
#include "seat/helper.h"
#include "treelanduserconfig.hpp"

WorkspaceAnimationController::WorkspaceAnimationController(QObject *parent)
    : QObject{ parent }
    , m_slideAnimation(new QSequentialAnimationGroup(this))
    , m_bounceAnimation(new QSequentialAnimationGroup(this))
    , m_posAnimation(new QPropertyAnimation(this))
    , m_bounceOutAnimation(new QPropertyAnimation(this))
    , m_bounceInAnimation(new QPropertyAnimation(this))
{
    m_posAnimation->setPropertyName("viewportPos");
    m_posAnimation->setEasingCurve(static_cast<QEasingCurve::Type>(Helper::instance()->config()->multitaskviewEasingCurveType()));
    m_posAnimation->setDuration(Helper::instance()->config()->multitaskviewAnimationDuration());
    m_posAnimation->setTargetObject(this);
    connect(m_slideAnimation, &QSequentialAnimationGroup::finished, this, [this] {
        if (m_needBounce) {
            startBounceAnimation();
        } else {
            finishAnimation();
        }
    });
    m_slideAnimation->addAnimation(m_posAnimation);
    m_bounceInAnimation->setTargetObject(this);
    m_bounceInAnimation->setEasingCurve(QEasingCurve::InOutExpo);
    m_bounceInAnimation->setDuration(Helper::instance()->config()->multitaskviewAnimationDuration() / 2);
    m_bounceInAnimation->setPropertyName("viewportPos");
    m_bounceOutAnimation->setTargetObject(this);
    m_bounceOutAnimation->setEasingCurve(QEasingCurve::InOutExpo);
    m_bounceOutAnimation->setDuration(Helper::instance()->config()->multitaskviewAnimationDuration() / 2);
    m_bounceOutAnimation->setPropertyName("viewportPos");
    m_bounceAnimation->addAnimation(m_bounceInAnimation);
    m_bounceAnimation->addAnimation(m_bounceOutAnimation);
    connect(m_bounceAnimation,
            &QSequentialAnimationGroup::finished,
            this,
            &WorkspaceAnimationController::finishAnimation);
}

qreal WorkspaceAnimationController::refWidth() const
{
    return m_refWidth;
}

void WorkspaceAnimationController::setRefWidth(qreal newRefWidth)
{
    if (qFuzzyCompare(m_refWidth, newRefWidth))
        return;
    m_refWidth = newRefWidth;
    Q_EMIT refWidthChanged();
    Q_EMIT refWrapChanged();
}

qreal WorkspaceAnimationController::refGap() const
{
    return m_refGap;
}

void WorkspaceAnimationController::setRefGap(qreal newRefGap)
{
    if (qFuzzyCompare(m_refGap, newRefGap))
        return;
    m_refGap = newRefGap;
    Q_EMIT refGapChanged();
    Q_EMIT refWrapChanged();
}

qreal WorkspaceAnimationController::refBounce() const
{
    return m_refBounce;
}

void WorkspaceAnimationController::setRefBounce(qreal newRefBounce)
{
    if (qFuzzyCompare(m_refBounce, newRefBounce))
        return;
    m_refBounce = newRefBounce;
    Q_EMIT refBounceChanged();
}

qreal WorkspaceAnimationController::bounceFactor() const
{
    return m_bounceFactor;
}

void WorkspaceAnimationController::setBounceFactor(qreal newBounceFactor)
{
    if (qFuzzyCompare(m_bounceFactor, newBounceFactor))
        return;
    m_bounceFactor = newBounceFactor;
    Q_EMIT bounceFactorChanged();
}

qreal WorkspaceAnimationController::refWrap() const
{
    return refWidth() + refGap();
}

bool WorkspaceAnimationController::running() const
{
    return m_running;
}

qreal WorkspaceAnimationController::viewportPos() const
{
    return m_viewportPos;
}

uint WorkspaceAnimationController::pendingWorkspaceIndex() const
{
    return m_pendingWorkspaceIndex;
}

void WorkspaceAnimationController::slideRunning(uint toWorkspaceIndex)
{
    if (!running())
        return;
    m_slideAnimation->stop();
    m_bounceAnimation->stop();
    m_animationInitial = viewportPos();
    m_animationDestination = refWrap() * toWorkspaceIndex;
    m_initialIndex = viewportPos() / refWrap();
    m_destinationIndex = toWorkspaceIndex;
    m_currentDirection = m_animationDestination > m_animationInitial ? Right : Left;
}

void WorkspaceAnimationController::slideNormal(uint fromWorkspaceIndex, uint toWorkspaceIndex)
{
    m_initialIndex = fromWorkspaceIndex;
    m_destinationIndex = toWorkspaceIndex;
    m_animationInitial = refWrap() * fromWorkspaceIndex;
    m_animationDestination = refWrap() * toWorkspaceIndex;
    m_currentDirection = (fromWorkspaceIndex < toWorkspaceIndex) ? Right : Left;
    setViewportPos(m_animationInitial);
}

void WorkspaceAnimationController::slide(uint fromWorkspaceIndex, uint toWorkspaceIndex)
{
    m_needBounce = false;
    slideRunning(toWorkspaceIndex);
    slideNormal(fromWorkspaceIndex, toWorkspaceIndex);
    startSlideAnimation();
}

void WorkspaceAnimationController::bounce(uint currentWorkspaceIndex, Direction direction)
{
    if (m_bounceAnimation->state() == QAbstractAnimation::Running)
        return;
    if (m_slideAnimation->state() != QAbstractAnimation::Running) {
        m_initialIndex = currentWorkspaceIndex;
        m_destinationIndex = currentWorkspaceIndex;
        m_currentDirection = direction;
        m_animationInitial = refWrap() * m_initialIndex;
        m_animationDestination = refWrap() * m_destinationIndex;
        startBounceAnimation();
    } else {
        m_needBounce = true;
    }
}

qreal WorkspaceAnimationController::gestureObstruction(qreal gestureValue)
{
    static constexpr qreal k = 10.0;
    return (bounceFactor() / M_PI) * std::atan(k * gestureValue);
}

void WorkspaceAnimationController::startGestureSlide(qreal cb, bool bounce)
{
    qreal offset = bounce ? gestureObstruction(cb) : cb;
    qreal pos = m_animationInitial + refWrap() * offset;

    setViewportPos(pos);
}

void WorkspaceAnimationController::startSlideAnimation()
{
    m_posAnimation->setStartValue(m_animationInitial);
    m_posAnimation->setEndValue(m_animationDestination);
    m_slideAnimation->start();
    setRunning(true);
}

void WorkspaceAnimationController::startBounceAnimation()
{
    auto bounceDestination =
        m_animationDestination + (m_currentDirection == Right ? refBounce() : -refBounce());
    m_bounceInAnimation->setStartValue(m_animationDestination);
    m_bounceInAnimation->setEndValue(bounceDestination);
    m_bounceOutAnimation->setStartValue(bounceDestination);
    m_bounceOutAnimation->setEndValue(m_animationDestination);
    m_bounceAnimation->start();
    setRunning(true);
}

void WorkspaceAnimationController::setRunning(bool running)
{
    if (running == m_running)
        return;
    m_running = running;
    Q_EMIT runningChanged();
}

void WorkspaceAnimationController::finishAnimation()
{
    Q_EMIT finished();
    setRunning(false);
}

void WorkspaceAnimationController::setViewportPos(qreal newViewportPos)
{
    if (qFuzzyCompare(m_viewportPos, newViewportPos))
        return;
    m_viewportPos = newViewportPos;
    Q_EMIT viewportPosChanged();
}
