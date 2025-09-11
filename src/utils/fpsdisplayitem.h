// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QQuickPaintedItem>
#include <QColor>
#include <QString>

class FpsDisplayItem : public QQuickPaintedItem
{
public:
    explicit FpsDisplayItem(QQuickItem *parent = nullptr);
    void paint(QPainter *painter) override;
    void setFpsText(const QString &text, const QColor &color);
    void setAvgFpsText(const QString &text, const QColor &color = QColor(255, 255, 255));
    void setShadowColor(const QColor &color);

private:
    QString m_fpsText = "Current FPS: 0";
    QString m_avgFpsText = "Maximum FPS: 0";
    QColor m_textColor = QColor(0, 0, 0);
    QColor m_shadowColor = QColor(255, 255, 255, 150);
};
