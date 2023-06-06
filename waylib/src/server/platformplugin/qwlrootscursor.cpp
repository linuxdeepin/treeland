// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwlrootscursor.h"
#include "wcursor.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

QWlrootsCursor::QWlrootsCursor()
    : QPlatformCursor()
{
    windowCursor = WCursor::defaultCursor();
}

#ifndef QT_NO_CURSOR
void QWlrootsCursor::changeCursor(QCursor *windowCursor, QWindow *)
{
    this->windowCursor = windowCursor ? *windowCursor : WCursor::defaultCursor();
    if (!activeCursor)
        return;

    activeCursor->setCursor(this->windowCursor);
}

void QWlrootsCursor::setOverrideCursor(const QCursor &cursor)
{
    if (activeCursor)
        activeCursor->setCursor(cursor);
}

void QWlrootsCursor::clearOverrideCursor()
{
    if (activeCursor)
        activeCursor->setCursor(windowCursor); // default type
}
#endif

QPoint QWlrootsCursor::pos() const
{
    return activeCursor ? activeCursor->position().toPoint() : QPoint();
}

void QWlrootsCursor::setPos(const QPoint &pos)
{
    if (activeCursor)
        activeCursor->setPosition(pos);
}

void QWlrootsCursor::addCursor(WCursor *cursor)
{
    cursors.append(cursor);
    if (!activeCursor)
        activeCursor = cursor;
}

void QWlrootsCursor::removeCursor(WCursor *cursor)
{
    bool ok = cursors.removeOne(cursor);
    Q_ASSERT(ok);

    if (cursor == activeCursor) {
        activeCursor = nullptr;
        if (!cursors.isEmpty())
            activeCursor = cursors.first();
    }
}

WAYLIB_SERVER_END_NAMESPACE
