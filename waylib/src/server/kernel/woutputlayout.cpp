// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputlayout.h"
#include "private/woutputlayout_p.h"
#include "woutput.h"

extern "C" {
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/box.h>
}

#include <QRect>

#include <utility>

WAYLIB_SERVER_BEGIN_NAMESPACE

WOutputLayoutPrivate::WOutputLayoutPrivate(WOutputLayout *qq)
    : WWrapObjectPrivate(qq)
{
    wl_list_init(&destroy.link);
    wl_list_init(&change.link);
}

WOutputLayoutPrivate::~WOutputLayoutPrivate()
{
    for (auto o : std::as_const(outputs)) {
        o->setLayout(nullptr);
    }
}

void WOutputLayoutPrivate::instantRelease()
{
    if (!handle)
        return;

    auto *layout = std::exchange(handle, nullptr);
    for (wl_listener *listener : { &destroy, &change }) {
        if (!wl_list_empty(&listener->link)) {
            wl_list_remove(&listener->link);
            wl_list_init(&listener->link);
        }
    }
    wlr_output_layout_destroy(layout);
}

void WOutputLayoutPrivate::handleDestroy(wl_listener *listener, [[maybe_unused]] void *data)
{
    WOutputLayoutPrivate *self;
    self = wl_container_of(listener, self, destroy);
    wl_list_remove(&self->destroy.link);
    wl_list_init(&self->destroy.link);
    if (!wl_list_empty(&self->change.link)) {
        wl_list_remove(&self->change.link);
        wl_list_init(&self->change.link);
    }
    self->handle = nullptr;
    self->q_func()->safeDeleteLater();
}

void WOutputLayoutPrivate::handleChange(wl_listener *listener, [[maybe_unused]] void *data)
{
    WOutputLayoutPrivate *self;
    self = wl_container_of(listener, self, change);
    self->updateImplicitSize();
    Q_EMIT self->q_func()->changed();
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
    wlr_output_layout_get_box(handle, nullptr, &tmp_box);
    auto newSize = QRect(tmp_box.x, tmp_box.y, tmp_box.width, tmp_box.height);

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
    dd.handle = wlr_output_layout_create(server->handle());
    Q_ASSERT(dd.handle);
    dd.destroy.notify = WOutputLayoutPrivate::handleDestroy;
    wl_signal_add(&dd.handle->events.destroy, &dd.destroy);
    dd.change.notify = WOutputLayoutPrivate::handleChange;
    wl_signal_add(&dd.handle->events.change, &dd.change);
}

WOutputLayout::WOutputLayout(WServer *server)
    : WOutputLayout(*new WOutputLayoutPrivate(this), server)
{

}

wlr_output_layout *WOutputLayout::handle() const
{
    W_DC(WOutputLayout);
    return d->handle;
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
    wlr_output_layout_add(d->handle, output->handle(), pos.x(), pos.y());
    d->doAdd(output);
}

void WOutputLayout::autoAdd(WOutput *output)
{
    W_D(WOutputLayout);
    output->setLayout(this);
    wlr_output_layout_add_auto(d->handle, output->handle());
    d->doAdd(output);
}

void WOutputLayout::move(WOutput *output, const QPoint &pos)
{
    W_D(WOutputLayout);
    Q_ASSERT(d->outputs.contains(output));
    Q_ASSERT(output->layout());

    if (output->position() == pos)
        return;

    wlr_output_layout_add(d->handle, output->handle(), pos.x(), pos.y());

    d->updateImplicitSize();
}

void WOutputLayout::remove(WOutput *output)
{
    W_D(WOutputLayout);
    Q_ASSERT(d->outputs.contains(output));
    d->outputs.removeOne(output);

    wlr_output_layout_remove(d->handle, output->handle());
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
        wlr_output_layout_get_box(d->handle, o->handle(), &tmp);
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
