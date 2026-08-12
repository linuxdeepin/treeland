// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputlayout.h"
#include "private/woutputlayout_p.h"
#include "woutput.h"

#include <wlr_all.h>

#include <QRect>

WAYLIB_SERVER_BEGIN_NAMESPACE

WOutputLayoutPrivate::WOutputLayoutPrivate(WOutputLayout *qq)
    : WObjectPrivate(qq)
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

    QObject::connect(output, &WOutput::effectiveSizeChanged, q, [this] {
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
    wlr_output_layout_get_box(handle(), nullptr, &tmp_box);
    const QRect newSize(tmp_box.x, tmp_box.y, tmp_box.width, tmp_box.height);

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
    : QObject(server)
    , WObject(dd, server)
{
    auto h = wlr_output_layout_create(server->handle());
    Q_ASSERT(h);
    h->data = d_func();
    d_func()->m_handle = h;
}

WOutputLayout::WOutputLayout(WServer *server)
    : WOutputLayout(*new WOutputLayoutPrivate(this), server)
{

}

wlr_output_layout *WOutputLayout::handle() const
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
    wlr_output_layout_add(d->handle(), output->handle(), pos.x(), pos.y());
    d->doAdd(output);
}

void WOutputLayout::autoAdd(WOutput *output)
{
    W_D(WOutputLayout);
    output->setLayout(this);
    wlr_output_layout_add_auto(d->handle(), output->handle());
    d->doAdd(output);
}

void WOutputLayout::move(WOutput *output, const QPoint &pos)
{
    W_D(WOutputLayout);
    Q_ASSERT(d->outputs.contains(output));
    Q_ASSERT(output->layout());

    if (output->position() == pos)
        return;

    wlr_output_layout_add(d->handle(), output->handle(), pos.x(), pos.y());

    d->updateImplicitSize();
}

void WOutputLayout::remove(WOutput *output)
{
    W_D(WOutputLayout);
    Q_ASSERT(d->outputs.contains(output));
    d->outputs.removeOne(output);

    wlr_output_layout_remove(d->handle(), output->handle());
    output->setLayout(nullptr);
    output->disconnect(this);
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
        wlr_output_layout_get_box(d->handle(), o->handle(), &tmp);
        const QRect og(tmp.x, tmp.y, tmp.width, tmp.height);
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
