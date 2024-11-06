// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "wglobal.h"

#include <QQuickItem>
Q_MOC_INCLUDE(<wsurfaceitem.h>)

class ItemSelector;
WAYLIB_SERVER_BEGIN_NAMESPACE
class WSurfaceItem;
WAYLIB_SERVER_END_NAMESPACE

class WindowPicker : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString hint READ hint NOTIFY hintChanged FINAL)
    Q_PROPERTY(QQuickItem *hoveredItem READ hoveredItem NOTIFY hoveredItemChanged FINAL)
    Q_PROPERTY(QRectF selectionRegion READ selectionRegion NOTIFY selectionRegionChanged FINAL)

public:
    WindowPicker(QQuickItem *parent = nullptr);
    QString hint() const;
    void setHint(const QString &hint);
    QQuickItem *hoveredItem() const;
    QRectF selectionRegion() const;

Q_SIGNALS:
    void windowPicked(WAYLIB_SERVER_NAMESPACE::WSurfaceItem *window);
    void hintChanged();
    void hoveredItemChanged();
    void selectionRegionChanged();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    QString m_hint;
    ItemSelector *m_itemSelector;
};
