// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef DAMAGEOVERLAY_H
#define DAMAGEOVERLAY_H

#include <QQmlEngine>
#include <QQuickPaintedItem>
#include <QTimer>
#include <QVariantList>

class DamageOverlay : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList frames READ frames WRITE setFrames NOTIFY framesChanged)
    Q_PROPERTY(int historyDuration READ historyDuration WRITE setHistoryDuration
               NOTIFY historyDurationChanged)
    Q_PROPERTY(int refreshRate READ refreshRate WRITE setRefreshRate NOTIFY refreshRateChanged)

public:
    explicit DamageOverlay(QQuickItem *parent = nullptr);

    QVariantList frames() const { return m_frames; }
    void setFrames(const QVariantList &frames);

    int historyDuration() const { return m_historyDuration; }
    void setHistoryDuration(int duration);

    int refreshRate() const { return m_refreshRate; }
    void setRefreshRate(int refreshRate);

    void paint(QPainter *painter) override;

signals:
    void framesChanged();
    void historyDurationChanged();
    void refreshRateChanged();

private:
    void paintRects(QPainter *painter, const QVariantList &rects, const QColor &color);

    QVariantList m_frames;
    QTimer m_repaintTimer;
    int m_historyDuration = 200;
    int m_refreshRate = 60;
};

#endif
