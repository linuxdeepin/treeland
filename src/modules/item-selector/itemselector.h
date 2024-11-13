// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once
#include <wglobal.h>

#include <QQmlEngine>
#include <QQuickItem>
WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutputItem;
WAYLIB_SERVER_END_NAMESPACE

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
    WAYLIB_SERVER_NAMESPACE::WOutputItem *outputItem() const;
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
    QList<QPointer<WAYLIB_SERVER_NAMESPACE::WOutputItem>> m_outputItems;
    QPointer<Waylib::Server::WOutputItem> m_outputItem;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ItemSelector::ItemTypes)
