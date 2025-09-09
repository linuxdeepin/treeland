// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "fpsdisplaymanager.h"
#include "fpsdisplayitem.h"
#include <QQuickWindow>
#include <QQuickItem>
#include <QLoggingCategory>
#include <QScreen>
#include "common/treelandlogging.h"

namespace {
    constexpr int kMaxSamples = 120;       // 2 seconds at 60Hz
    constexpr int kUpdateIntervalMs = 500; // UI update frequency (ms)
}

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
    m_vsyncTimer.setSingleShot(false);
    connect(&m_vsyncTimer, &QTimer::timeout, this, &FpsDisplayManager::onVSyncTimer);
}

FpsDisplayManager::~FpsDisplayManager()
{
    destroyDisplayItem();
}

void FpsDisplayManager::setTargetWindow(QQuickWindow *window)
{
    if (m_targetWindow == window)
        return;

    destroyDisplayItem();
    m_targetWindow = window;

    if (m_targetWindow) {
        updateRefreshAndInterval();
        QObject::connect(m_targetWindow, &QQuickWindow::screenChanged,
                         this, &FpsDisplayManager::onScreenChanged);
        // Track size changes to keep FPS display geometry in sync
        QObject::connect(m_targetWindow, &QQuickWindow::widthChanged,
                         this, &FpsDisplayManager::updateDisplayItemGeometry);
        QObject::connect(m_targetWindow, &QQuickWindow::heightChanged,
                         this, &FpsDisplayManager::updateDisplayItemGeometry);
        if (m_targetWindow->contentItem()) {
            QObject::connect(m_targetWindow->contentItem(), &QQuickItem::widthChanged,
                             this, &FpsDisplayManager::updateDisplayItemGeometry);
            QObject::connect(m_targetWindow->contentItem(), &QQuickItem::heightChanged,
                             this, &FpsDisplayManager::updateDisplayItemGeometry);
        }
    }

    if (m_visible && m_targetWindow) {
        createDisplayItem();
    }
}

void FpsDisplayManager::show()
{
    if (m_visible && m_displayItem)
        return;

    m_visible = true;
    qCDebug(treelandFpsDisplay) << "FPS display shown";

    if (m_targetWindow && !m_displayItem) {
        createDisplayItem();
        m_updateTimer.start();
        m_vsyncTimer.start();
        reset();
    }
}

void FpsDisplayManager::hide()
{
    if (!m_visible)
        return;

    m_visible = false;
    qCDebug(treelandFpsDisplay) << "FPS display hidden";
    m_updateTimer.stop();
    m_vsyncTimer.stop();
    destroyDisplayItem();
}

void FpsDisplayManager::toggle()
{
    if (m_visible) {
        hide();
    } else {
        show();
    }
}

void FpsDisplayManager::onVSyncTimer()
{
    if (!m_visible)
        return;

    qint64 currentTime = m_timer.elapsed();

    if (m_lastVSyncTime_precise > 0) {
        qint64 actualInterval = currentTime - m_lastVSyncTime_precise;
        double expectedInterval = m_preciseVSyncInterval;

        if (actualInterval > expectedInterval * 1.5 || actualInterval < expectedInterval * 0.5) {
            m_lastVSyncTime_precise = currentTime;
            return;
        }
    }

    m_vSyncTimes.enqueue(currentTime);
    m_lastVSyncTime_precise = currentTime;

    while (m_vSyncTimes.size() > kMaxSamples)
        m_vSyncTimes.dequeue();
}

void FpsDisplayManager::reset()
{
    m_currentFps = m_displayRefreshRate;
    m_averageFps = m_displayRefreshRate;
    m_maximumFps = m_displayRefreshRate;
    m_lastUpdateTime = m_timer.elapsed();
    m_vSyncTimes.clear();
    m_lastVSyncTime_precise = 0;

    if (m_displayItem) {
        updateFpsText();
    }
}

void FpsDisplayManager::updateFps()
{
    if (!m_visible)
        return;

    if (m_vSyncTimes.isEmpty()) {
        // Clear or reset the FPS display when no vsync events have occurred
        if (m_displayItem) {
            auto displayItem = m_displayItem ? dynamic_cast<FpsDisplayItem*>(m_displayItem.data()) : nullptr;
            if (displayItem) {
                displayItem->setFpsText("Current FPS: N/A", m_style.text);
                displayItem->setAvgFpsText("Maximum FPS: N/A", m_style.text);
            }
        }
        return;
    }

    qint64 currentTime = m_timer.elapsed();
    qint64 timeDiff = currentTime - m_lastUpdateTime;

    if (timeDiff < kUpdateIntervalMs)
        return;

    qreal currentFps = 0.0;
    qreal averageFps = 0.0;

    if (m_vSyncTimes.size() >= 2) {
        qint64 totalTimeSpan = m_vSyncTimes.last() - m_vSyncTimes.first();
        if (totalTimeSpan > 0) {
            averageFps = ((m_vSyncTimes.size() - 1) * 1000.0) / totalTimeSpan;
        }

        if (m_vSyncTimes.size() >= 3) {
            qint64 recentTimeSpan = m_vSyncTimes.last() - m_vSyncTimes[m_vSyncTimes.size() - 3];
            if (recentTimeSpan > 0)
                currentFps = (2 * 1000.0) / recentTimeSpan;
        } else {
            currentFps = averageFps;
        }
    } else if (m_vSyncTimes.size() == 1) {
        currentFps = averageFps = m_displayRefreshRate;
    }

    if (currentFps > 0 || averageFps > 0) {
        const qreal smoothingFactor = 0.3;
        if (m_currentFps == 0.0) {
            m_currentFps = currentFps;
            m_averageFps = averageFps;
        } else {
            m_currentFps = m_currentFps * (1.0 - smoothingFactor) + currentFps * smoothingFactor;
            m_averageFps = m_averageFps * (1.0 - smoothingFactor) + averageFps * smoothingFactor;
        }
        m_currentFps = qMax(1.0, m_currentFps);
        m_averageFps = qMax(1.0, m_averageFps);

        if (m_currentFps > m_maximumFps)
            m_maximumFps = m_currentFps;
    }

    m_lastUpdateTime = currentTime;
    updateFpsText();
}

void FpsDisplayManager::detectDisplayRefreshRate()
{
    m_displayRefreshRate = 60;
    if (m_targetWindow) {
        if (auto screen = m_targetWindow->screen()) {
            qreal refreshRate = screen->refreshRate();
            if (refreshRate > 0) {
                m_displayRefreshRate = qRound(refreshRate);
                qCDebug(treelandFpsDisplay) << "Detected display refresh rate:" << m_displayRefreshRate << "Hz";
                return;
            } else {
                qCWarning(treelandFpsDisplay) << "Screen refresh rate is zero or negative (" << refreshRate << "); using default 60Hz";
            }
        }
    }

    QString envRefreshRate = qgetenv("TREELAND_REFRESH_RATE");
    if (!envRefreshRate.isEmpty()) {
        bool ok;
        int rate = envRefreshRate.toInt(&ok);
        if (ok && rate > 0 && rate <= 240) {
            m_displayRefreshRate = rate;
            qCDebug(treelandFpsDisplay) << "Using environment refresh rate:" << m_displayRefreshRate << "Hz";
        }
    }
}

void FpsDisplayManager::createDisplayItem()
{
    if (!m_targetWindow || m_displayItem)
        return;

    auto displayItem = new FpsDisplayItem(nullptr);
    displayItem->setParent(m_targetWindow);
    displayItem->setParentItem(m_targetWindow->contentItem());

    m_displayItem = displayItem;
    updateDisplayItemGeometry();
    displayItem->setZ(99999);
    updateFpsText();

    qreal w = displayItem->width();
    qreal h = displayItem->height();
    qreal scale = (w > 0.0) ? (w / 130.0) : 1.0;
    qCDebug(treelandFpsDisplay) << "FPS display item created at" << QPoint(displayItem->x(), displayItem->y())
                                << "size" << QSizeF(w, h) << "scale" << scale;

    QMetaObject::invokeMethod(this, [this]() {
        if (m_displayItem) {
            m_displayItem->setVisible(true);
            m_displayItem->setEnabled(false);
            m_displayItem->setFocus(false);
            m_displayItem->setActiveFocusOnTab(false);

            if (m_targetWindow && m_targetWindow->contentItem()) {
                auto contentItem = m_targetWindow->contentItem();
                if (!contentItem->hasActiveFocus() && !contentItem->hasFocus()) {
                    contentItem->setFocus(true);
                }
            }
        }
    }, Qt::QueuedConnection);
}

void FpsDisplayManager::updateDisplayItemGeometry()
{
    if (!m_targetWindow || !m_targetWindow->contentItem() || !m_displayItem)
        return;

    auto content = m_targetWindow->contentItem();
    qreal windowWidth = content->width();
    qreal scaleFactor = (windowWidth > 2560) ? 1.5 : ((windowWidth > 1920) ? 1.2 : 1.0);

    qreal displayWidth = 130 * scaleFactor;
    qreal displayHeight = 45 * scaleFactor;
    qreal margin = 20 * scaleFactor;
    qreal topOffset = 40 * scaleFactor;

    m_displayItem->setWidth(displayWidth);
    m_displayItem->setHeight(displayHeight);
    m_displayItem->setX(windowWidth - displayWidth - margin);
    m_displayItem->setY(topOffset);
}

void FpsDisplayManager::destroyDisplayItem()
{
    if (m_displayItem) {
        qCDebug(treelandFpsDisplay) << "FPS display item destroyed";
        m_displayItem->deleteLater();
        m_displayItem = nullptr;
    }
}

void FpsDisplayManager::updateFpsText()
{
    auto displayItem = m_displayItem ? dynamic_cast<FpsDisplayItem*>(m_displayItem.data()) : nullptr;
    if (!displayItem)
        return;

    QString currentFpsText = QString("Current FPS: %1").arg(qRound(m_currentFps));
    QString maximumFpsText = QString("Maximum FPS: %1").arg(qRound(m_maximumFps));
    
    displayItem->setFpsText(currentFpsText, m_style.text);
    displayItem->setAvgFpsText(maximumFpsText, m_style.text);
    displayItem->setShadowColor(m_style.shadow);
}

void FpsDisplayManager::onScreenChanged(QScreen *screen)
{
    Q_UNUSED(screen);
    int oldRate = m_displayRefreshRate;
    updateRefreshAndInterval();
    if (m_displayRefreshRate != oldRate) {
        if (m_vsyncTimer.isActive()) {
            m_vsyncTimer.stop();
            m_vsyncTimer.start();
        }

        m_vSyncTimes.clear();
        m_lastVSyncTime_precise = 0;

        m_currentFps = m_displayRefreshRate;
        m_averageFps = m_displayRefreshRate;
        updateDisplayItemGeometry();
    }
}


void FpsDisplayManager::updateRefreshAndInterval()
{
    detectDisplayRefreshRate();
    double preciseInterval = 1000.0 / m_displayRefreshRate;
    int vsyncInterval = qRound(preciseInterval);
    m_preciseVSyncInterval = preciseInterval;

    if (m_displayRefreshRate == 60) {
        vsyncInterval = 17;
        m_preciseVSyncInterval = 16.666667;
    } else if (m_displayRefreshRate == 90) {
        vsyncInterval = 11;
        m_preciseVSyncInterval = 11.111111;
    } else if (m_displayRefreshRate == 120) {
        vsyncInterval = 8;
        m_preciseVSyncInterval = 8.333333;
    } else if (m_displayRefreshRate == 144) {
        vsyncInterval = 7;
        m_preciseVSyncInterval = 6.944444;
    }

    qCDebug(treelandFpsDisplay) << "Refresh rate updated:" << m_displayRefreshRate << "Hz, VSync interval:" << vsyncInterval << "ms";
    m_vsyncTimer.setInterval(vsyncInterval);
}
