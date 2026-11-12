// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "framegeometry.h"
#include <QPointer>
#include <QQuickItem>

// Keep the mask at (0, 0): QQuickItem subtracts its position before calling contains().
class FrameInputMask : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(FrameGeometry geometry MEMBER m_geometry NOTIFY geometryChanged FINAL)
    Q_PROPERTY(QQuickItem* frameItem READ frameItem WRITE setFrameItem NOTIFY frameItemChanged FINAL)
    Q_PROPERTY(QQuickItem* target READ target WRITE setTarget NOTIFY targetChanged FINAL)
    Q_PROPERTY(qreal resizeMargin MEMBER m_resizeMargin NOTIFY resizeMarginChanged FINAL)

public:
    explicit FrameInputMask(QQuickItem *parent = nullptr);
    QQuickItem *frameItem() const { return m_frameItem; }
    void setFrameItem(QQuickItem *item);
    QQuickItem *target() const { return m_target; }
    void setTarget(QQuickItem *item);
    bool contains(const QPointF &point) const override;
    Q_INVOKABLE Qt::Edges edgesAt(const QPointF &point) const;

Q_SIGNALS:
    void geometryChanged();
    void frameItemChanged();
    void targetChanged();
    void resizeMarginChanged();

private:
    bool inTarget(const QPointF &point) const;
    FrameGeometry m_geometry;
    QPointer<QQuickItem> m_frameItem;
    QPointer<QQuickItem> m_target;
    qreal m_resizeMargin = 0;
};
