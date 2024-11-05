// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once
#include <QQmlEngine>
#include <QQuickItem>

class ItemSelector : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QRectF selectionRegion READ selectionRegion NOTIFY selectionRegionChanged FINAL)
    Q_PROPERTY(QQuickItem* hoveredItem READ hoveredItem NOTIFY hoveredItemChanged FINAL)
    Q_PROPERTY(ItemTypes selectionTypeHint READ selectionTypeHint WRITE setSelectionTypeHint NOTIFY selectionTypeHintChanged FINAL)

public:
    enum ItemType
    {
        Output = 0x1,
        Window = 0x2,
        Surface = 0x4,
    };

    Q_FLAG(ItemType);
    Q_DECLARE_FLAGS(ItemTypes, ItemType)

    ItemSelector(QQuickItem *parent = nullptr);
    ~ItemSelector();

    QRectF selectionRegion() const;
    QQuickItem *hoveredItem() const;
    void setSelectionTypeHint(ItemTypes newSelectionTypeHint);
    ItemTypes selectionTypeHint() const;

Q_SIGNALS:
    void selectionRegionChanged();
    void hoveredItemChanged();
    void selectionTypeHintChanged();

protected:
    void hoverMoveEvent(QHoverEvent *event) override;
    void itemChange(ItemChange, const ItemChangeData &) override;

private:
    void setSelectionRegion(const QRectF &newSelectionRegion);
    void setHoveredItem(QQuickItem *newHoveredItem);
    void updateSelectableItems();
    void checkHoveredItem(QPointF pos);

    QPointer<QQuickItem> m_hoveredItem{};
    QRectF m_selectionRegion{};
    QList<QPointer<QQuickItem>> m_selectableItems{};
    ItemTypes m_selectionTypeHint{ ItemType::Window | ItemType::Output | ItemType::Surface };
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ItemSelector::ItemTypes)
