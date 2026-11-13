// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QObject>
#include <QRectF>
#include <QtQmlIntegration/qqmlintegration.h>

class FrameGeometry
{
    Q_GADGET
    QML_VALUE_TYPE(frameGeometry)
    Q_PROPERTY(QRectF rect READ rect CONSTANT)
    Q_PROPERTY(qreal radius READ radius CONSTANT)

public:
    FrameGeometry() = default;
    FrameGeometry(const QRectF &rect, qreal radius);

    QRectF rect() const { return m_rect; }
    qreal radius() const { return m_radius; }
    bool contains(const QPointF &point) const;
    Qt::Edges resizeEdgesAt(const QPointF &point, qreal margin) const;
    bool operator==(const FrameGeometry &) const = default;

private:
    QRectF m_rect;
    qreal m_radius = 0;
};

Q_DECLARE_METATYPE(FrameGeometry)
