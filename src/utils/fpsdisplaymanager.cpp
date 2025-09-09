// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "fpsdisplaymanager.h"
#include <QDateTime>
#include <QQuickWindow>
#include <woutputrenderwindow.h>
#include <woutput.h>
#include <woutputviewport.h>

FpsDisplayManager::FpsDisplayManager(QObject *parent)
    : QObject(parent)
    , m_updateTimer(this)
    , m_vsyncTimer(this)
{
    m_timer.start();

    m_updateTimer.setInterval(kUpdateIntervalMs);
    m_updateTimer.setSingleShot(false);
    connect(&m_updateTimer, &QTimer::timeout, this, &FpsDisplayManager::updateFps);

    updateRefreshAndInterval();
    m_vsyncTimer.setInterval(qRound(m_preciseVSyncInterval));
    m_vsyncTimer.setSingleShot(false);
    connect(&m_vsyncTimer, &QTimer::timeout, this, &FpsDisplayManager::onVSyncTimer);
}

FpsDisplayManager::~FpsDisplayManager()
{
    m_updateTimer.stop();
    m_vsyncTimer.stop();

    if (m_targetWindow) {
        disconnect(m_targetWindow, nullptr, this, nullptr);
    }
}

void FpsDisplayManager::setTargetWindow(QQuickWindow *window)
{
    if (m_targetWindow == window)
        return;

    if (m_targetWindow) {
        disconnect(m_targetWindow, nullptr, this, nullptr);
    }

    invalidateCache();
    m_targetWindow = window;

    if (m_targetWindow) {
        updateRefreshAndInterval();
        connect(m_targetWindow, &QQuickWindow::screenChanged,
                this, &FpsDisplayManager::onScreenChanged, Qt::UniqueConnection);
    }
}

void FpsDisplayManager::start()
{
    reset();
    updateRefreshAndInterval();
    m_vsyncTimer.setInterval(qRound(m_preciseVSyncInterval));
    m_updateTimer.start();
    m_vsyncTimer.start();
}

void FpsDisplayManager::stop()
{
    m_updateTimer.stop();
    m_vsyncTimer.stop();
}

void FpsDisplayManager::onVSyncTimer()
{
    qint64 currentTime = m_timer.elapsed();
    if (m_lastVSyncTime_precise > 0) {
        qint64 actualInterval = currentTime - m_lastVSyncTime_precise;
        double expectedInterval = m_preciseVSyncInterval;

        // Filter out timer jitter: reject intervals shorter than 70% of expected
        if (actualInterval < expectedInterval * 0.7) {
            return;
        }

        if (actualInterval > expectedInterval * 2.5) {
            m_lastVSyncTime_precise = currentTime;
            return;
        }
    }

    m_vSyncTimes.enqueue(currentTime);
    m_lastVSyncTime_precise = currentTime;

    // Keep at least 30 samples or 1 second worth of data for accuracy
    int maxSamples = qMax(30, m_displayRefreshRate);
    while (m_vSyncTimes.size() > maxSamples) {
        m_vSyncTimes.dequeue();
    }
}

void FpsDisplayManager::reset()
{
    m_currentFps = 0.0;
    m_maximumFps = 0.0;
    m_lastUpdateTime = m_timer.elapsed();
    m_vSyncTimes.clear();
    m_lastVSyncTime_precise = 0;

    updateFpsText();
}

void FpsDisplayManager::updateFps()
{
    if (m_vSyncTimes.isEmpty()) {
        if (m_currentFps != 0 || m_maximumFps != 0) {
            m_currentFps = 0;
            m_maximumFps = 0;
            updateFpsText();
        }
        return;
    }

    qint64 currentTime = m_timer.elapsed();
    qint64 timeDiff = currentTime - m_lastUpdateTime;

    if (timeDiff < kUpdateIntervalMs) {
        return;
    }

    qreal calculatedFps = 0.0;
    if (m_vSyncTimes.size() >= 2) {
        qint64 totalTimeSpan = m_vSyncTimes.last() - m_vSyncTimes.first();
        if (totalTimeSpan > 0) {
            calculatedFps = ((m_vSyncTimes.size() - 1) * 1000.0) / totalTimeSpan;
        }
    }

    if (calculatedFps <= 0) {
        calculatedFps = m_displayRefreshRate;
    }

    calculatedFps = qBound(1.0, calculatedFps, (qreal)m_displayRefreshRate);
    if (qAbs(calculatedFps - m_displayRefreshRate) < m_displayRefreshRate * 0.1) {
        calculatedFps = m_displayRefreshRate;
    }

    // Apply exponential smoothing to reduce fluctuations (15% new, 85% old)
    const qreal smoothingFactor = 0.15;
    if (m_currentFps == 0.0) {
        m_currentFps = calculatedFps;
    } else {
        m_currentFps = m_currentFps * (1.0 - smoothingFactor) + calculatedFps * smoothingFactor;
    }

    m_currentFps = qMin(m_currentFps, (qreal)m_displayRefreshRate);
    if (m_currentFps > m_maximumFps) {
        qreal maxAllowedFps = m_displayRefreshRate * 1.05; // Allow 5% measurement error
        m_maximumFps = qMin(m_currentFps, maxAllowedFps);
    }

    m_maximumFps = qMin(m_maximumFps, (qreal)m_displayRefreshRate * 1.05);
    m_lastUpdateTime = currentTime;
    updateFpsText();
}

void FpsDisplayManager::detectDisplayRefreshRate()
{
    if (m_targetWindow) {
        if (auto output = getOutputForWindow()) {
            if (auto qwoutput = output->handle()) {
                if (auto mode = qwoutput->preferred_mode()) {
                    int32_t refresh = mode->refresh;
                    if (refresh > 0) {
                        m_displayRefreshRate = qRound(refresh / 1000.0);
                    }
                }
            }
        }
    }

    // Environment variable override for debugging/testing
    QString envRefreshRate = qgetenv("TREELAND_REFRESH_RATE");
    if (!envRefreshRate.isEmpty()) {
        bool ok;
        int rate = envRefreshRate.toInt(&ok);
        if (ok && rate > 0 && rate <= 240) {
            m_displayRefreshRate = rate;
        }
    }
}

void FpsDisplayManager::updateFpsText()
{
    int current = currentFps();
    int maximum = maximumFps();

    if (current != m_lastReportedCurrentFps) {
        m_lastReportedCurrentFps = current;
        emit currentFpsChanged();
    }

    if (maximum != m_lastReportedMaximumFps) {
        m_lastReportedMaximumFps = maximum;
        emit maximumFpsChanged();
    }
}

void FpsDisplayManager::onScreenChanged(QScreen *screen)
{
    Q_UNUSED(screen);

    invalidateCache();
    int oldRate = m_displayRefreshRate;
    updateRefreshAndInterval();

    if (m_displayRefreshRate != oldRate) {
        if (m_vsyncTimer.isActive()) {
            m_vsyncTimer.stop();
            m_vsyncTimer.start();
        }

        m_vSyncTimes.clear();
        m_lastVSyncTime_precise = 0;
        m_currentFps = 0.0;
    }
}

void FpsDisplayManager::updateTimerIntervals()
{
    m_preciseVSyncInterval = 1000.0 / m_displayRefreshRate;
    int vsyncInterval = qRound(m_preciseVSyncInterval);

    // Use pre-calculated intervals for common refresh rates to avoid rounding errors
    if (m_displayRefreshRate == 60) {
        vsyncInterval = 17;  // 16.67ms rounded up for stability
    } else if (m_displayRefreshRate == 90) {
        vsyncInterval = 11;  // 11.11ms
    } else if (m_displayRefreshRate == 120) {
        vsyncInterval = 8;   // 8.33ms
    } else if (m_displayRefreshRate == 144) {
        vsyncInterval = 7;   // 6.94ms
    }

    vsyncInterval = qMax(1, vsyncInterval);
    m_vsyncTimer.setInterval(vsyncInterval);
}

void FpsDisplayManager::updateRefreshAndInterval()
{
    int oldRate = m_displayRefreshRate;
    detectDisplayRefreshRate();

    if (m_displayRefreshRate != oldRate) {
        updateTimerIntervals();
        emit refreshRateChanged();
    }
}

WOutput *FpsDisplayManager::findBestOutput(const QVector<WOutput*> &outputs) const
{
    if (outputs.isEmpty()) {
        return nullptr;
    }

    if (outputs.size() == 1) {
        return outputs.first();
    }

    for (auto output : outputs) {
        if (output && output->isEnabled()) {
            return output;
        }
    }
    WOutput *bestOutput = outputs.first();
    int maxRefreshRate = 0;

    for (auto output : outputs) {
        if (!output) continue;

        if (auto qwoutput = output->handle()) {
            if (auto mode = qwoutput->preferred_mode()) {
                int refreshRate = qRound(mode->refresh / 1000.0);
                if (refreshRate > maxRefreshRate) {
                    maxRefreshRate = refreshRate;
                    bestOutput = output;
                }
            }
        }
    }

    return bestOutput;
}

void FpsDisplayManager::invalidateCache()
{
    m_cachedOutput = nullptr;
    m_cacheTimestamp = 0;
}

WOutput *FpsDisplayManager::getOutputForWindow() const
{
    if (!m_targetWindow)
        return nullptr;

    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    if (m_cachedOutput && (currentTime - m_cacheTimestamp) < kCacheValidityMs) {
        return m_cachedOutput;
    }

    auto renderWindow = qobject_cast<WOutputRenderWindow *>(m_targetWindow);
    if (!renderWindow) {
        return nullptr;
    }

    QVector<WOutput*> outputs;
    for (auto child : renderWindow->children()) {
        if (auto viewport = qobject_cast<WOutputViewport*>(child)) {
            if (auto output = viewport->output()) {
                outputs.append(output);
            }
        }
    }

    WOutput *bestOutput = nullptr;
    if (!outputs.isEmpty()) {
        bestOutput = findBestOutput(outputs);
    }

    m_cachedOutput = bestOutput;
    m_cacheTimestamp = currentTime;
    return bestOutput;
}
