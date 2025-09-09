// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QQueue>
#include <QPointer>
#include <QColor>

class QQuickItem;
class QQuickWindow;
class QScreen;

class FpsDisplayManager : public QObject
{
    Q_OBJECT

public:
    explicit FpsDisplayManager(QObject *parent = nullptr);
    ~FpsDisplayManager();

    void setTargetWindow(QQuickWindow *window);
    void show();
    void hide();
    void toggle();
    void reset();
    bool isVisible() const { return m_visible; }
    void setTextColor(const QColor &color) { m_style.text = color; }
    void setShadowColor(const QColor &color) { m_style.shadow = color; }

private Q_SLOTS:
    void updateFps();
    void onVSyncTimer();
    void onScreenChanged(QScreen *screen);

private:
    void updateRefreshAndInterval();
    void createDisplayItem();
    void destroyDisplayItem();
    void updateFpsText();
    void detectDisplayRefreshRate();
    void updateDisplayItemGeometry();

    QPointer<QQuickWindow> m_targetWindow;
    QPointer<QQuickItem> m_displayItem;

    QElapsedTimer m_timer;
    QTimer m_updateTimer;
    QTimer m_vsyncTimer;
    QQueue<qint64> m_vSyncTimes;

    qreal m_currentFps = 0.0;
    qreal m_averageFps = 0.0;
    qreal m_maximumFps = 0.0;
    qint64 m_lastUpdateTime = 0;
    qint64 m_lastVSyncTime_precise = 0;

    int m_displayRefreshRate = 60;
    double m_preciseVSyncInterval = 16.67;
    bool m_visible = false;

    struct Style {
        QColor text = QColor(0, 0, 0);
        QColor shadow = QColor(255, 255, 255, 150);
    } m_style;
};
