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

#include <qwoutputlayout.h>
#include <qwoutput.h>

#include <QPoint>
#include <QDebug>

extern "C" {
#include <wlr/types/wlr_output_layout.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputLayoutPrivate : public WObjectPrivate
{
public:
    WOutputLayoutPrivate(WOutputLayout *qq)
        : WObjectPrivate(qq)
        , handle(new QWOutputLayout(qq))
    {
        connect();
    }

    ~WOutputLayoutPrivate() {
        Q_FOREACH(auto i, outputList) {
            i->setLayout(nullptr);
        }

        delete handle;
    }

    // begin slot function
    void on_change(void*);
    // end slot function

    void connect();

    W_DECLARE_PUBLIC(WOutputLayout)

    QWOutputLayout *handle;
    QVector<WOutput*> outputList;
    QVector<WCursor*> cursorList;
};

void WOutputLayoutPrivate::connect()
{
    QObject::connect(handle, &QWOutputLayout::change, q_func(), [this] {
        Q_FOREACH(auto output, outputList) {
            if (!output->nativeInterface<QWOutput>()->handle()) // During destroy application
                continue;
            Q_EMIT output->positionChanged(output->position());
        }
    });
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
    d->handle->addAuto(output->nativeInterface<QWOutput>()->handle());
    output->setLayout(this);
    d->outputList << output;
    Q_EMIT outputAdded(output);
}

void WOutputLayout::add(WOutput *output, const QPoint &pos)
{
    W_D(WOutputLayout);
    d->handle->add(output->nativeInterface<QWOutput>()->handle(), pos);
    output->setLayout(this);
    Q_ASSERT(!d->outputList.contains(output));
    d->outputList << output;
    Q_EMIT outputAdded(output);
}

void WOutputLayout::move(WOutput *output, const QPoint &pos)
{
    W_D(WOutputLayout);
    d->handle->move(output->nativeInterface<QWOutput>()->handle(), pos);
}

void WOutputLayout::remove(WOutput *output)
{
    W_D(WOutputLayout);
    d->handle->remove(output->nativeInterface<QWOutput>()->handle());
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
    return WOutput::fromHandle<QWOutput>(QWOutput::from(d->handle->outputAt(pos)));
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
