// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquickseat_p.h"
#include "wseat.h"
#include "wquickcursor.h"
#include "woutput.h"

#include <qwcursor.h>
#include <qwoutput.h>

#include <QRect>

extern "C" {
#define static
#include <wlr/types/wlr_cursor.h>
#undef static
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickSeatPrivate : public WObjectPrivate
{
public:
    WQuickSeatPrivate(WQuickSeat *qq)
        : WObjectPrivate(qq)
    {

    }

    inline QWCursor *qwcursor() const {
        return cursor->handle();
    }
    inline void resetCursorRegion() {
        // TODO fix qwlroots: using nullptr to wlr_cursor_map_to_region if the QRect is Null
        wlr_cursor_map_to_region(qwcursor()->handle(), nullptr);
    }

    void updateCursorMap();

    W_DECLARE_PUBLIC(WQuickSeat)

    WSeat *seat = nullptr;
    WSeatEventFilter *eventFilter = nullptr;
    WSurface *keyboardFocus = nullptr;
    QString name;
    QList<WOutput*> outputs;
    WQuickCursor *cursor = nullptr;
};

void WQuickSeatPrivate::updateCursorMap()
{
    auto cursor = qwcursor();
    Q_ASSERT(cursor);

    if (outputs.size() > 1) {
        QPoint minPos(INT_MAX, INT_MAX);
        QPoint maxPos(0, 0);

        for (auto o : outputs) {
            QRect rect = o->layout()->getBox(o->handle());

            if (rect.x() < minPos.x())
                minPos.rx() = rect.x();
            if (rect.y() < minPos.y())
                minPos.ry() = rect.y();

            if (rect.right() > maxPos.x())
                maxPos.rx() = rect.right();
            if (rect.bottom() < maxPos.y())
                maxPos.ry() = rect.bottom();
        }

        cursor->mapToRegion(QRect(minPos, maxPos));
        cursor->mapToOutput(nullptr);
    } else if (outputs.size() == 1) {
        cursor->mapToOutput(outputs.first()->handle());
        resetCursorRegion();
    } else {
        cursor->mapToOutput(nullptr);
        resetCursorRegion();
    }
}

WQuickSeat::WQuickSeat(QObject *parent)
    : WQuickWaylandServerInterface(parent)
    , WObject(*new WQuickSeatPrivate(this))
{

}

QString WQuickSeat::name() const
{
    W_DC(WQuickSeat);

    return d->name;
}

void WQuickSeat::setName(const QString &newName)
{
    W_D(WQuickSeat);

    d->name = newName;
}

WQuickCursor *WQuickSeat::cursor() const
{
    W_DC(WQuickSeat);
    return d->cursor;
}

void WQuickSeat::setCursor(WQuickCursor *cursor)
{
    W_D(WQuickSeat);
    if (d->cursor == cursor)
        return;

    d->cursor = cursor;

    if (d->seat) {
        d->seat->setCursor(cursor);
        d->updateCursorMap();
    }
    Q_EMIT cursorChanged();
}

WSeat *WQuickSeat::seat() const
{
    W_DC(WQuickSeat);
    return d->seat;
}

WSeatEventFilter *WQuickSeat::eventFilter() const
{
    W_DC(WQuickSeat);
    return d->eventFilter;
}

void WQuickSeat::setEventFilter(WSeatEventFilter *newEventFilter)
{
    W_D(WQuickSeat);
    if (d->eventFilter == newEventFilter)
        return;
    d->eventFilter = newEventFilter;
    if (d->seat)
        d->seat->setEventFilter(newEventFilter);
    Q_EMIT eventFilterChanged();
}


WSurface *WQuickSeat::keyboardFocus() const
{
    W_DC(WQuickSeat);
    return d->keyboardFocus;
}

void WQuickSeat::setKeyboardFocus(WSurface *newKeyboardFocus)
{
    W_D(WQuickSeat);
    if (d->keyboardFocus == newKeyboardFocus)
        return;
    d->keyboardFocus = newKeyboardFocus;
    if (d->seat)
        d->seat->setKeyboardFocusTarget(newKeyboardFocus);

    Q_EMIT keyboardFocusChanged();
}

void WQuickSeat::setKeyboardFocusWindow(QWindow *window)
{
    W_D(WQuickSeat);
    d->seat->setKeyboardFocusTarget(window);
}

void WQuickSeat::addDevice(WInputDevice *device)
{
    W_D(WQuickSeat);

    d->seat->attachInputDevice(device);
}

void WQuickSeat::removeDevice(WInputDevice *device)
{
    W_D(WQuickSeat);

    d->seat->detachInputDevice(device);
}

void WQuickSeat::addOutput(WOutput *output)
{
    W_D(WQuickSeat);
    Q_ASSERT(!d->outputs.contains(output));
    d->outputs.append(output);

    if (d->seat && d->seat->cursor())
        d->updateCursorMap();
}

void WQuickSeat::removeOutput(WOutput *output)
{
    W_D(WQuickSeat);
    Q_ASSERT(d->outputs.contains(output));
    d->outputs.removeOne(output);

    if (d->seat && d->seat->cursor())
        d->updateCursorMap();
}

void WQuickSeat::create()
{
    WQuickWaylandServerInterface::create();

    W_D(WQuickSeat);
    Q_ASSERT(!d->name.isEmpty());
    d->seat = server()->attach<WSeat>(d->name.toUtf8());
    d->seat->setOwnsSocket(ownsSocket());
    if (d->eventFilter)
        d->seat->setEventFilter(d->eventFilter);
    if (d->keyboardFocus)
        d->seat->setKeyboardFocusTarget(d->keyboardFocus);

    Q_EMIT seatChanged();
}

void WQuickSeat::polish()
{
    W_D(WQuickSeat);
    if (d->cursor) {
        d->seat->setCursor(d->cursor);
        d->updateCursorMap();
    }
}

void WQuickSeat::ownsSocketChange()
{
    W_D(WQuickSeat);
    if (d->seat)
        d->seat->setOwnsSocket(ownsSocket());
}

WAYLIB_SERVER_END_NAMESPACE
