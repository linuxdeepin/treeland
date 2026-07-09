// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QElapsedTimer>
#include <QQuickItem>
#include <qqmlregistration.h>

class QQuickWindow;

class ShaderFrameClock : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(qreal playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(qreal time READ time WRITE setTime NOTIFY timeChanged)

public:
    explicit ShaderFrameClock(QQuickItem *parent = nullptr);

    bool running() const;
    void setRunning(bool running);

    qreal playbackRate() const;
    void setPlaybackRate(qreal playbackRate);

    qreal time() const;
    void setTime(qreal time);

Q_SIGNALS:
    void runningChanged();
    void playbackRateChanged();
    void timeChanged();

protected:
    void itemChange(ItemChange change, const ItemChangeData &data) override;

private:
    void connectWindow(QQuickWindow *window);
    void disconnectWindow();
    void tick();
    void requestNextFrame();

    QQuickWindow *m_window = nullptr;
    QElapsedTimer m_elapsedTimer;
    bool m_running = true;
    qreal m_playbackRate = 1.0;
    qreal m_time = 0.0;
};
