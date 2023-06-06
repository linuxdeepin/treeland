// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputlayoutitem.h"
#include "woutputrenderwindow.h"

#include <qwoutput.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputLayoutItemPrivate : public WObjectPrivate
{
public:
    WOutputLayoutItemPrivate(WOutputLayoutItem *qq)
        : WObjectPrivate(qq)
    {

    }

    W_DECLARE_PUBLIC(WOutputLayoutItem)
    QList<WOutputViewport*> outputs;
};

WOutputLayoutItem::WOutputLayoutItem(QQuickItem *parent)
    : QQuickItem(parent)
    , WObject(*new WOutputLayoutItemPrivate(this))
{

}

QList<WOutputViewport *> WOutputLayoutItem::outputs() const
{
    W_DC(WOutputLayoutItem);
    return d->outputs;
}

void WOutputLayoutItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    W_D(WOutputLayoutItem);
    auto oldOutputs = d->outputs;
    auto renderWindow = qobject_cast<WOutputRenderWindow*>(window());
    d->outputs = renderWindow->getIntersectedOutputs(QRectF(parentItem()->mapToScene(newGeometry.topLeft()), newGeometry.size()));

    bool changed = false;

    for (auto o : d->outputs) {
        if (oldOutputs.removeOne(o))
            continue;
        Q_EMIT enterOutput(o);
        changed = true;
    }

    for (auto o : oldOutputs) {
        Q_EMIT leaveOutput(o);
        changed = true;
    }

    if (changed)
        Q_EMIT outputsChanged();
}

WAYLIB_SERVER_END_NAMESPACE
