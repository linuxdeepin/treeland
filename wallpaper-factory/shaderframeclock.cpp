// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shaderframeclock.h"

#include <QQuickWindow>

ShaderFrameClock::ShaderFrameClock(QQuickItem *parent)
    : QQuickItem(parent)
{
}

bool ShaderFrameClock::running() const
{
    return m_running;
}

void ShaderFrameClock::setRunning(bool running)
{
    if (m_running == running)
        return;

    m_running = running;
    m_elapsedTimer.restart();
    Q_EMIT runningChanged();
    requestNextFrame();
}

qreal ShaderFrameClock::playbackRate() const
{
    return m_playbackRate;
}

void ShaderFrameClock::setPlaybackRate(qreal playbackRate)
{
    if (qFuzzyCompare(m_playbackRate, playbackRate))
        return;

    m_playbackRate = playbackRate;
    Q_EMIT playbackRateChanged();
    requestNextFrame();
}

qreal ShaderFrameClock::time() const
{
    return m_time;
}

void ShaderFrameClock::setTime(qreal time)
{
    if (qFuzzyCompare(m_time, time))
        return;

    m_time = time;
    Q_EMIT timeChanged();
    requestNextFrame();
}

void ShaderFrameClock::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);

    if (change == ItemSceneChange)
        connectWindow(data.window);
}

void ShaderFrameClock::connectWindow(QQuickWindow *window)
{
    if (m_window == window)
        return;

    disconnectWindow();
    m_window = window;
    m_elapsedTimer.restart();

    if (!m_window)
        return;

    connect(m_window, &QQuickWindow::frameSwapped,
            this, &ShaderFrameClock::tick);
    requestNextFrame();
}

void ShaderFrameClock::disconnectWindow()
{
    if (!m_window)
        return;

    disconnect(m_window, nullptr, this, nullptr);
    m_window = nullptr;
}

void ShaderFrameClock::tick()
{
    if (!m_elapsedTimer.isValid())
        m_elapsedTimer.restart();

    const qint64 elapsed = m_elapsedTimer.nsecsElapsed();
    m_elapsedTimer.restart();

    if (m_running && m_playbackRate > 0.0) {
        m_time += (elapsed / 1000000000.0) * m_playbackRate;
        Q_EMIT timeChanged();
    }

    requestNextFrame();
}

void ShaderFrameClock::requestNextFrame()
{
    if (!m_window || !m_running)
        return;

    m_window->requestUpdate();
}
