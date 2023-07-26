// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickcoordmapper_p.h"
#include "wquickobserver.h"

#include <private/qquickitem_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

WQuickCoordMapperHelper::WQuickCoordMapperHelper(QQuickItem *target)
    : QObject(target)
    , m_target(target)
{

}

WQuickCoordMapper *WQuickCoordMapperHelper::get(WQuickObserver *target)
{
    for (auto i : list) {
        if (i->m_target == target)
            return i;
    }

    auto mapper = new WQuickCoordMapper(target, m_target);
    connect(mapper, &WQuickCoordMapper::destroyed, this, [this] {
        list.removeOne(qobject_cast<WQuickCoordMapper*>(QObject::sender()));
    });
    list.append(mapper);

    return mapper;
}

WQuickCoordMapperAttached::WQuickCoordMapperAttached(QQuickItem *target)
    : QObject(target)
    , m_target(target)
{
    connect(target, &QQuickItem::parentChanged, this, &WQuickCoordMapperAttached::helperChanged);
}

WQuickCoordMapperHelper *WQuickCoordMapperAttached::helper() const
{
    auto parent = m_target->parentItem();
    if (!parent)
        return nullptr;

    auto helper = parent->findChild<WQuickCoordMapperHelper*>(QString(), Qt::FindDirectChildrenOnly);
    if (!helper)
        helper = new WQuickCoordMapperHelper(parent);

    return helper;
}

class WQuickCoordMapperPrivate : public QQuickItemPrivate
{
public:
    bool transformChanged(QQuickItem *transformedItem) override {
        Q_Q(WQuickCoordMapper);
        q->updatePosition();

        return QQuickItemPrivate::transformChanged(transformedItem);
    }

    Q_DECLARE_PUBLIC(WQuickCoordMapper)
};

WQuickCoordMapper::WQuickCoordMapper(WQuickObserver *target, QQuickItem *parent)
    : QQuickItem(*new WQuickCoordMapperPrivate(), parent)
    , m_target(target)
{
    setFlag(QQuickItem::ItemObservesViewport);

    connect(target, &WQuickObserver::maybeGlobalPositionChanged,
            this, &WQuickCoordMapper::updatePosition);

    bindableWidth().setBinding(target->bindableWidth().makeBinding());
    bindableHeight().setBinding(target->bindableHeight().makeBinding());
}

void WQuickCoordMapper::updatePosition()
{
    auto parent = parentItem();
    Q_ASSERT(parent);
    const QPointF &pos = m_target->globalPosition();
    setPosition(parent->mapFromGlobal(pos));
}

WQuickCoordMapperAttached *WQuickCoordMapper::qmlAttachedProperties(QObject *target)
{
    auto item = qobject_cast<QQuickItem*>(target);
    if (!item)
        return nullptr;
    return new WQuickCoordMapperAttached(item);
}

WAYLIB_SERVER_END_NAMESPACE
