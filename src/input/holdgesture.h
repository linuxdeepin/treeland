// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "gestures.h"

class QTimer;

class HoldGesture : public Gesture
{
    Q_OBJECT
public:
    explicit HoldGesture(QObject *parent = nullptr);
    ~HoldGesture();

    void setInterval(int msec);
    void startTimer();
    void stopTimer();
    bool isActive() const;

    bool minimumFingerCountIsRelevant() const;
    void setMinimumFingerCount(uint count);
    uint minimumFingerCount() const;

    bool maximumFingerCountIsRelevant() const;
    void setMaximumFingerCount(uint count);
    uint maximumFingerCount() const;

private:
    QTimer *m_holdTimer;
    bool m_minimumFingerCountRelevant = false;
    uint m_minimumFingerCount = 0;
    bool m_maximumFingerCountRelevant = false;
    uint m_maximumFingerCount = 0;
};
