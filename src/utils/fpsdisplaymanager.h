// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QQueue>
#include <QPointer>

class QQuickItem;
class QQuickWindow;
class QScreen;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class FpsDisplayManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentFps READ currentFps NOTIFY currentFpsChanged)
    Q_PROPERTY(int maximumFps READ maximumFps NOTIFY maximumFpsChanged)
    Q_PROPERTY(int displayRefreshRate READ displayRefreshRate NOTIFY refreshRateChanged)
    QML_ELEMENT

public:
    explicit FpsDisplayManager(QObject *parent = nullptr);
    ~FpsDisplayManager();

    Q_INVOKABLE void setTargetWindow(QQuickWindow *window);
    // TODO: Add setTargetOutput(WOutput* output) method for multi-screen FPS display support
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void reset();

    int currentFps() const { return qRound(m_currentFps); }
    int maximumFps() const { return qRound(m_maximumFps); }
    int displayRefreshRate() const { return m_displayRefreshRate; }

signals:
    void currentFpsChanged();
    void maximumFpsChanged();
    void refreshRateChanged();

private Q_SLOTS:
    void updateFps();
    void onVSyncTimer();
    void onScreenChanged(QScreen *screen);

private:
    void updateRefreshAndInterval();
    void updateTimerIntervals();
    void updateFpsText();
    void detectDisplayRefreshRate();
    WOutput *getOutputForWindow() const;
    WOutput *findBestOutput(const QVector<WOutput*> &outputs) const;
    void invalidateCache();

    QPointer<QQuickWindow> m_targetWindow;

    QElapsedTimer m_timer;
    QTimer m_updateTimer;
    QTimer m_vsyncTimer;
    QQueue<qint64> m_vSyncTimes;

    qreal m_currentFps = 0.0;
    qreal m_averageFps = 0.0;
    qreal m_maximumFps = 0.0;
    qint64 m_lastUpdateTime = 0;
    qint64 m_lastVSyncTime_precise = 0;

    int m_displayRefreshRate = 60;              // Display refresh rate in Hz
    double m_preciseVSyncInterval = 16.67;      // Precise VSync interval in milliseconds
    // Constants
    static constexpr int kUpdateIntervalMs = 500;     // UI update frequency in milliseconds
    static constexpr qint64 kCacheValidityMs = 5000;  // Cache validity: 5 seconds

    // Cache for change detection
    int m_lastReportedCurrentFps = -1;
    int m_lastReportedMaximumFps = -1;

    // Output caching for performance optimization
    mutable QPointer<WOutput> m_cachedOutput;
    mutable qint64 m_cacheTimestamp = 0;
};
