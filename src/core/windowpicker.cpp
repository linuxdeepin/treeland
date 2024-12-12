// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "windowpicker.h"

#include "modules/item-selector/itemselector.h"

#include <wsurfaceitem.h>

WAYLIB_SERVER_USE_NAMESPACE
WindowPicker::WindowPicker(QQuickItem *parent)
    : QQuickItem(parent)
    , m_itemSelector(new ItemSelector(this))
{
    setCursor(Qt::CrossCursor);
    setAcceptedMouseButtons(Qt::LeftButton);
    m_itemSelector->setSelectionTypeHint(ItemSelector::Window);
    connect(m_itemSelector,
            &ItemSelector::hoveredItemChanged,
            this,
            &WindowPicker::hoveredItemChanged);
    connect(m_itemSelector,
            &ItemSelector::selectionRegionChanged,
            this,
            &WindowPicker::selectionRegionChanged);
}

QString WindowPicker::hint() const
{
    return m_hint;
}

void WindowPicker::setHint(const QString &hint)
{
    if (hint == m_hint)
        return;
    m_hint = hint;
    Q_EMIT hintChanged();
}

QQuickItem *WindowPicker::hoveredItem() const
{
    return m_itemSelector->hoveredItem();
}

QRectF WindowPicker::selectionRegion() const
{
    return m_itemSelector->selectionRegion();
}

void WindowPicker::mousePressEvent(QMouseEvent *event)
{
    auto window = qobject_cast<WSurfaceItem *>(m_itemSelector->hoveredItem());
    Q_EMIT windowPicked(window);
}
