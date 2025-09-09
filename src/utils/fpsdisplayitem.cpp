// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "fpsdisplayitem.h"
#include <QPainter>
#include <QFont>

FpsDisplayItem::FpsDisplayItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setEnabled(false);
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);
    setAcceptTouchEvents(false);
    setFlag(ItemAcceptsInputMethod, false);
    setFlag(ItemAcceptsDrops, false);
    setFlag(ItemIsFocusScope, false);
    setFlag(ItemHasContents, true);

    setVisible(false);
    setWidth(130);
    setHeight(45);
}

void FpsDisplayItem::paint(QPainter *painter)
{
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::TextAntialiasing);

    QFont font;
    qreal scaleFactor = 1.0;
    if (width() > 150) {
        scaleFactor = width() / 130.0;
    }
    int fontSize = qRound(14 * scaleFactor);

    font.setPixelSize(fontSize);
    font.setWeight(QFont::Normal);
    font.setHintingPreference(QFont::PreferFullHinting);
    painter->setFont(font);

    qreal lineHeight = height() / 2.2;
    qreal lineSpacing = height() / 2.0;

    painter->setPen(m_shadowColor);
    painter->drawText(QRectF(1, 1, width(), lineHeight), Qt::AlignLeft | Qt::AlignVCenter, m_fpsText);
    painter->drawText(QRectF(1, lineSpacing + 1, width(), lineHeight), Qt::AlignLeft | Qt::AlignVCenter, m_avgFpsText);

    painter->setPen(m_textColor);
    painter->drawText(QRectF(0, 0, width(), lineHeight), Qt::AlignLeft | Qt::AlignVCenter, m_fpsText);
    painter->drawText(QRectF(0, lineSpacing, width(), lineHeight), Qt::AlignLeft | Qt::AlignVCenter, m_avgFpsText);
}

void FpsDisplayItem::setFpsText(const QString &text, const QColor &color)
{
    if (m_fpsText != text || m_textColor != color) {
        m_fpsText = text;
        m_textColor = color;
        update();
    }
}

void FpsDisplayItem::setAvgFpsText(const QString &text, const QColor &color)
{
    if (m_avgFpsText != text || m_textColor != color) {
        m_avgFpsText = text;
        m_textColor = color;
        update();
    }
}

void FpsDisplayItem::setShadowColor(const QColor &color)
{
    if (m_shadowColor != color) {
        m_shadowColor = color;
        update();
    }
}
