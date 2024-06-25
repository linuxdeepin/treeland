// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputlayout.h"
#include "private/woutputlayout_p.h"
#include "woutput.h"

#include <qwoutput.h>
#include <QRect>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

WOutputLayoutPrivate::WOutputLayoutPrivate(WOutputLayout *qq)
    : WObjectPrivate(qq)
{

}

void WOutputLayoutPrivate::updateImplicitSize()
{
    W_Q(WOutputLayout);

    const auto &newSize = q->getBox(nullptr);

    if (implicitWidth != newSize.x() + newSize.width()) {
        implicitWidth = newSize.x() + newSize.width();
        Q_EMIT q->implicitWidthChanged();
    }
    if (implicitHeight != newSize.y() + newSize.height()) {
        implicitHeight = newSize.y() + newSize.height();
        Q_EMIT q->implicitHeightChanged();
    }
}

WOutputLayout::WOutputLayout(WOutputLayoutPrivate &dd, QObject *parent)
    : QWOutputLayout(parent)
    , WObject(dd)
{

}

WOutputLayout::WOutputLayout(QObject *parent)
    : WOutputLayout(*new WOutputLayoutPrivate(this), parent)
{

}

QList<WOutput*> WOutputLayout::outputs() const
{
    W_DC(WOutputLayout);
    return d->outputs;
}

void WOutputLayout::add(WOutput *output, const QPoint &pos)
{
    W_D(WOutputLayout);
    Q_ASSERT(!d->outputs.contains(output));
    d->outputs.append(output);

    QWOutputLayout::add(output->handle(), pos);
    output->setLayout(this);

    output->safeConnect(&WOutput::effectiveSizeChanged, this, [d](){
        d->updateImplicitSize();
    });
    d->updateImplicitSize();

    Q_EMIT outputAdded(output);
}

void WOutputLayout::move(WOutput *output, const QPoint &pos)
{
    W_D(WOutputLayout);
    Q_ASSERT(d->outputs.contains(output));
    Q_ASSERT(output->layout());

    if (output->position() == pos)
        return;

    QWOutputLayout::move(output->handle(), pos);

    d->updateImplicitSize();
}

void WOutputLayout::remove(WOutput *output)
{
    W_D(WOutputLayout);
    Q_ASSERT(d->outputs.contains(output));
    d->outputs.removeOne(output);

    QWOutputLayout::remove(output->handle());
    output->setLayout(nullptr);
    output->safeDisconnect(this);
    d->updateImplicitSize();

    Q_EMIT outputRemoved(output);
}

QList<WOutput*> WOutputLayout::getIntersectedOutputs(const QRect &geometry) const
{
    W_DC(WOutputLayout);

    QList<WOutput*> outputs;

    for (auto o : d->outputs) {
        const QRect og = getBox(o->handle());
        if (og.intersects(geometry))
            outputs << o;
    }

    return outputs;
}

int WOutputLayout::implicitWidth() const
{
    W_DC(WOutputLayout);
    return d->implicitWidth;
}

int WOutputLayout::implicitHeight() const
{
    W_DC(WOutputLayout);
    return d->implicitHeight;
}

WAYLIB_SERVER_END_NAMESPACE
