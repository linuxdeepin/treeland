// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "gestures.h"

#include <QObject>

class WorkspaceSwipeGesture : public QObject
{
    Q_OBJECT
public:
    WorkspaceSwipeGesture(SwipeGesture::Direction direction, uint finger, QObject *parent = nullptr);
    ~WorkspaceSwipeGesture();

    void destroy();

protected:
    void moveSlide(qreal cb);
    void moveDischarge();

    void activeTriggered();
    void deactivateTriggered();

private:
    qreal m_desktopOffset;
    int m_fromId = 0;
    int m_toId = 0;
    bool m_slideEnable = false;
    bool m_slideBounce = false;

    SwipeGesture *forwardGesture;
    SwipeGesture *backwordGesture;
};

Q_DECLARE_METATYPE(WorkspaceSwipeGesture *)
