// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "holdgesture.h"

#include <QTimer>

HoldGesture::HoldGesture(QObject *parent)
    : Gesture(parent)
    , m_holdTimer(new QTimer(this))
{
    m_holdTimer->setSingleShot(true);
    connect(m_holdTimer, &QTimer::timeout, this, &HoldGesture::longPressed);
}

bool HoldGesture::minimumFingerCountIsRelevant() const
{
    return m_minimumFingerCountRelevant;
}

void HoldGesture::setMinimumFingerCount(uint count)
{
    if (count == m_minimumFingerCount) {
        return;
    }
    m_minimumFingerCount = count;
    m_minimumFingerCountRelevant = true;
}

uint HoldGesture::minimumFingerCount() const
{
    return m_minimumFingerCount;
}

bool HoldGesture::maximumFingerCountIsRelevant() const
{
    return m_maximumFingerCountRelevant;
}

void HoldGesture::setMaximumFingerCount(uint count)
{
    if (count == m_maximumFingerCount) {
        return;
    }
    m_maximumFingerCount = count;
    m_maximumFingerCountRelevant = true;
}

uint HoldGesture::maximumFingerCount() const
{
    return m_maximumFingerCount;
}

HoldGesture::~HoldGesture()
{
    if (m_holdTimer != nullptr) {
        m_holdTimer->stop();
    }
}

void HoldGesture::startTimer()
{
    m_holdTimer->start();
}

void HoldGesture::setInterval(int msec)
{
    m_holdTimer->setInterval(msec);
}

void HoldGesture::stopTimer()
{
    m_holdTimer->stop();
}

bool HoldGesture::isActive() const
{
    return m_holdTimer->isActive();
}
