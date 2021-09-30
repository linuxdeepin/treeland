/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "woutputlayout.h"
#include "wbackend.h"
#include "woutput.h"
#include "wsignalconnector.h"


#include <QPoint>
#include <QDebug>

extern "C" {
#include <wlr/types/wlr_output_layout.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputLayoutPrivate : public WObjectPrivate
{
public:
    WOutputLayoutPrivate(WOutputLayout *qq)
        : WObjectPrivate(qq)
        , handle(wlr_output_layout_create())
    {
        connect();
    }

    ~WOutputLayoutPrivate() {
        Q_FOREACH(auto i, outputList) {
            i->setLayout(nullptr);
        }

        sc.invalidate();
        wlr_output_layout_destroy(handle);
    }

    // begin slot function
    void on_change(void*);
    // end slot function

    void connect();

    W_DECLARE_PUBLIC(WOutputLayout)

    wlr_output_layout *handle;
    QVector<WOutput*> outputList;
    QVector<WCursor*> cursorList;
    WSignalConnector sc;
};

void WOutputLayoutPrivate::on_change(void *)
{
    Q_FOREACH(auto output, outputList) {
        Q_EMIT output->positionChanged(output->position());
    }
}

void WOutputLayoutPrivate::connect()
{
    sc.connect(&handle->events.change, this,
               &WOutputLayoutPrivate::on_change);
}

WOutputLayout::WOutputLayout(QObject *parent)
    : QObject(parent)
    , WObject(*new WOutputLayoutPrivate(this))
{

}

WOutputLayoutHandle *WOutputLayout::handle() const
{
    W_DC(WOutputLayout);
    return reinterpret_cast<WOutputLayoutHandle*>(d->handle);
}

void WOutputLayout::add(WOutput *output)
{
    W_D(WOutputLayout);
    wlr_output_layout_add_auto(d->handle, output->nativeInterface<wlr_output>());
    output->setLayout(this);
    d->outputList << output;
    Q_EMIT outputAdded(output);
}

void WOutputLayout::add(WOutput *output, const QPoint &pos)
{
    W_D(WOutputLayout);
    wlr_output_layout_add(d->handle, output->nativeInterface<wlr_output>(),
                          pos.x(), pos.y());
    output->setLayout(this);
    Q_ASSERT(!d->outputList.contains(output));
    d->outputList << output;
    Q_EMIT outputAdded(output);
}

void WOutputLayout::move(WOutput *output, const QPoint &pos)
{
    W_D(WOutputLayout);
    wlr_output_layout_move(d->handle, output->nativeInterface<wlr_output>(),
                           pos.x(), pos.y());
}

void WOutputLayout::remove(WOutput *output)
{
    W_D(WOutputLayout);
    wlr_output_layout_remove(d->handle, output->nativeInterface<wlr_output>());
    output->setLayout(nullptr);
    Q_ASSERT(d->outputList.contains(output));
    d->outputList.removeOne(output);
    Q_EMIT outputRemoved(output);
}

QVector<WOutput *> WOutputLayout::outputList() const
{
    W_DC(WOutputLayout);
    return d->outputList;
}

WOutput *WOutputLayout::at(const QPointF &pos) const
{
    W_DC(WOutputLayout);
    Q_FOREACH(auto output, d->outputList) {
        auto woutput = output->nativeInterface<wlr_output>();
        auto geometry = wlr_output_layout_get_box(d->handle, woutput);
        if (wlr_box_contains_point(geometry, pos.x(), pos.y()))
            return output;
    }

    return nullptr;
}

void WOutputLayout::addCursor(WCursor *cursor)
{
    W_D(WOutputLayout);
    d->cursorList << cursor;
}

void WOutputLayout::removeCursor(WCursor *cursor)
{
    W_D(WOutputLayout);
    d->cursorList.removeOne(cursor);
}

QVector<WCursor *> WOutputLayout::cursorList() const
{
    W_DC(WOutputLayout);
    return d->cursorList;
}

WAYLIB_SERVER_END_NAMESPACE
