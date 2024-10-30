// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QObject>
#include <QPropertyAnimation>
#include <QQmlEngine>
#include <QSequentialAnimationGroup>

class WorkspaceAnimationController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
public:
    explicit WorkspaceAnimationController(QObject *parent = nullptr);
    Q_PROPERTY(qreal refWidth READ refWidth WRITE setRefWidth NOTIFY refWidthChanged FINAL)
    Q_PROPERTY(qreal refGap READ refGap WRITE setRefGap NOTIFY refGapChanged FINAL)
    Q_PROPERTY(qreal refBounce READ refBounce WRITE setRefBounce NOTIFY refBounceChanged FINAL)
    Q_PROPERTY(qreal bounceFactor READ bounceFactor WRITE setBounceFactor NOTIFY bounceFactorChanged FINAL)
    Q_PROPERTY(qreal refWrap READ refWrap NOTIFY refWrapChanged FINAL)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged FINAL)
    Q_PROPERTY(qreal viewportPos READ viewportPos WRITE setViewportPos NOTIFY viewportPosChanged FINAL)
    Q_PROPERTY(uint pendingWorkspaceIndex READ pendingWorkspaceIndex NOTIFY pendingWorkspaceIndexChanged FINAL)
    qreal refWidth() const;
    void setRefWidth(qreal newRefWidth);
    qreal refGap() const;
    void setRefGap(qreal newRefGap);
    qreal refBounce() const;
    void setRefBounce(qreal newRefBounce);
    qreal bounceFactor() const;
    void setBounceFactor(qreal newBounceFactor);
    qreal refWrap() const;
    bool running() const;
    qreal viewportPos() const;
    void setViewportPos(qreal newViewportPos);
    uint pendingWorkspaceIndex() const;

    enum Direction
    {
        Left,
        Right
    };
    Q_ENUM(Direction)

Q_SIGNALS:
    void refWidthChanged();
    void refGapChanged();
    void refBounceChanged();
    void bounceFactorChanged();
    void refWrapChanged();
    void runningChanged();
    void viewportPosChanged();
    void pendingWorkspaceIndexChanged();
    void finished();

public Q_SLOTS:
    void slideRunning(uint toWorkspaceIndex);
    void slideNormal(uint fromWorkspaceIndex, uint toWorkspaceIndex);
    void slide(uint fromWorkspaceIndex, uint toWorkspaceIndex);
    void bounce(uint currentWorkspaceIndex, Direction direction);
    void setRunning(bool running);
    void startSlideAnimation();
    void startGestureSlide(qreal cb, bool bounce = false);

private:
    void startBounceAnimation();
    void finishAnimation();
    qreal gestureObstruction(qreal gestureValue);

    qreal m_refWidth = 1920;
    qreal m_refGap = 30;
    qreal m_refBounce = 384;
    qreal m_bounceFactor = 0.3;
    bool m_running = false;
    qreal m_viewportPos = 0;
    uint m_pendingWorkspaceIndex = 0;
    qreal m_animationInitial = 0;
    qreal m_animationDestination = 0;
    bool m_needBounce = false;
    uint m_initialIndex = 0;
    uint m_destinationIndex = 0;
    Direction m_currentDirection = Left;
    QSequentialAnimationGroup *m_slideAnimation;
    QSequentialAnimationGroup *m_bounceAnimation;
    QPropertyAnimation *m_posAnimation;
    QPropertyAnimation *m_bounceOutAnimation;
    QPropertyAnimation *m_bounceInAnimation;
};
