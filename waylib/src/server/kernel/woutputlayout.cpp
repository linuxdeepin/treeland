// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputlayout.h"
#include "private/woutputlayout_p.h"
#include "woutput.h"

#include <qwoutput.h>
#include <qwbox.h>
#include <qwoutputlayout.h>
#include <qwdisplay.h>

#include <QRect>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

WOutputLayoutPrivate::WOutputLayoutPrivate(WOutputLayout *qq)
    : WWrapObjectPrivate(qq)
{

}

WOutputLayoutPrivate::~WOutputLayoutPrivate()
{
    for (auto o : std::as_const(outputs)) {
        o->setLayout(nullptr);
    }
}

void WOutputLayoutPrivate::doAdd(WOutput *output)
{
    Q_ASSERT(!outputs.contains(output));
    outputs.append(output);

    W_Q(WOutputLayout);
    Q_ASSERT(output->layout() == q);

    output->safeConnect(&WOutput::effectiveSizeChanged, q, [this] {
        updateImplicitSize();
    });
    updateImplicitSize();

    Q_EMIT q->outputAdded(output);
    Q_EMIT q->outputsChanged();
}

void WOutputLayoutPrivate::updateImplicitSize()
{
    W_Q(WOutputLayout);

    wlr_box tmp_box;
    handle()->get_box(nullptr, &tmp_box);
    auto newSize = qw_box(tmp_box).toQRect();

    if (implicitWidth != newSize.x() + newSize.width()) {
        implicitWidth = newSize.x() + newSize.width();
        Q_EMIT q->implicitWidthChanged();
    }
    if (implicitHeight != newSize.y() + newSize.height()) {
        implicitHeight = newSize.y() + newSize.height();
        Q_EMIT q->implicitHeightChanged();
    }
}

WOutputLayout::WOutputLayout(WOutputLayoutPrivate &dd, WServer *server)
    : WWrapObject(dd, server)
{
    auto h = new qw_output_layout(*server->handle());
    initHandle(h);

    W_D(WOutputLayout);

    handle()->set_data(this, this);
}

WOutputLayout::WOutputLayout(WServer *server)
    : WOutputLayout(*new WOutputLayoutPrivate(this), server)
{

}

qw_output_layout *WOutputLayout::handle() const
{
    W_DC(WOutputLayout);
    return d->handle();
}

const QList<WOutput*> &WOutputLayout::outputs() const
{
    W_DC(WOutputLayout);
    return d->outputs;
}

void WOutputLayout::add(WOutput *output, const QPoint &pos)
{
    W_D(WOutputLayout);
    output->setLayout(this);
    d->handle()->add(output->nativeHandle(), pos.x(), pos.y());
    d->doAdd(output);
}

void WOutputLayout::autoAdd(WOutput *output)
{
    W_D(WOutputLayout);
    output->setLayout(this);
    d->handle()->add_auto(output->nativeHandle());
    d->doAdd(output);
}

void WOutputLayout::move(WOutput *output, const QPoint &pos)
{
    W_D(WOutputLayout);
    Q_ASSERT(d->outputs.contains(output));
    Q_ASSERT(output->layout());

    if (output->position() == pos)
        return;

    d->handle()->add(output->nativeHandle(), pos.x(), pos.y());

    d->updateImplicitSize();
}

void WOutputLayout::remove(WOutput *output)
{
    W_D(WOutputLayout);
    Q_ASSERT(d->outputs.contains(output));
    d->outputs.removeOne(output);

    d->handle()->remove(output->nativeHandle());
    output->setLayout(nullptr);
    output->safeDisconnect(this);
    d->updateImplicitSize();

    Q_EMIT outputRemoved(output);
    Q_EMIT outputsChanged();
}

QList<WOutput*> WOutputLayout::getIntersectedOutputs(const QRect &geometry) const
{
    W_DC(WOutputLayout);

    QList<WOutput*> outputs;

    for (auto o : std::as_const(d->outputs)) {
        wlr_box tmp;
        d->handle()->get_box(o->nativeHandle(), &tmp);
        const QRect og = qw_box(tmp).toQRect();
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
