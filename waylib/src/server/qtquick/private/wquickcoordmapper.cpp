// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickcoordmapper_p.h"
#include "wquickobserver.h"

#include <private/qquickitem_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WQuickCoordMapperPrivate : public QQuickItemPrivate
{
public:
    WQuickCoordMapperPrivate(WQuickObserver *target)
        : target(target)
    {
    }

    bool transformChanged(QQuickItem *transformedItem) override {
        Q_Q(WQuickCoordMapper);
        q->updatePosition();

        return QQuickItemPrivate::transformChanged(transformedItem);
    }

    static WQuickCoordMapperPrivate *get(WQuickCoordMapper *item) {
        return item->d_func();
    }

    static const WQuickCoordMapperPrivate *get(const WQuickCoordMapper *item) {
        return item->d_func();
    }

    WQuickObserver *target;

    Q_DECLARE_PUBLIC(WQuickCoordMapper)
};

WQuickCoordMapperHelper::WQuickCoordMapperHelper(QQuickItem *target)
    : QObject(target)
    , m_target(target)
{

}

WQuickCoordMapper *WQuickCoordMapperHelper::get(WQuickObserver *target)
{
    for (auto i : std::as_const(list)) {
        if (WQuickCoordMapperPrivate::get(i)->target == target)
            return i;
    }

    auto mapper = new WQuickCoordMapper(target, m_target);
    connect(mapper, &WQuickCoordMapper::destroyed, this, [this] {
        list.removeOne(static_cast<WQuickCoordMapper*>(QObject::sender()));
    });

    connect(target, &WQuickCoordMapper::destroyed, mapper, &WQuickCoordMapper::deleteLater);

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

WQuickCoordMapper::WQuickCoordMapper(WQuickObserver *target, QQuickItem *parent)
    : QQuickItem(*new WQuickCoordMapperPrivate(target), parent)
{
    setFlag(QQuickItem::ItemObservesViewport);

    connect(target, &WQuickObserver::maybeGlobalPositionChanged,
            this, &WQuickCoordMapper::updatePosition);

    bindableWidth().setBinding(target->bindableWidth().makeBinding());
    bindableHeight().setBinding(target->bindableHeight().makeBinding());
}

void WQuickCoordMapper::updatePosition()
{
    Q_D(WQuickCoordMapper);

    auto parent = parentItem();
    Q_ASSERT(parent);
    const QPointF &pos = d->target->globalPosition();
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
