// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "waylibcursor.h"
#include "waylibwindow.h"
#include "wcursor.h"
#include "types.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WaylibCursor::WaylibCursor()
    : QPlatformCursor()
{

}

#ifndef QT_NO_CURSOR
void WaylibCursor::changeCursor(QCursor *windowCursor, QWindow *window)
{
    if (!window || !QW::RenderWindow::check(window))
        return;

    auto cursor = static_cast<WaylibRenderWindow*>(window->handle())->lastActiveCursor;
    if (!cursor)
        return;

    cursor->setCursor(windowCursor ? *windowCursor : WCursor::defaultCursor());
}

void WaylibCursor::setOverrideCursor([[maybe_unused]] const QCursor &cursor)
{
    return;
}

void WaylibCursor::clearOverrideCursor()
{

}
#endif

QPoint WaylibCursor::pos() const
{
    return cursors.isEmpty() ? QPoint() : cursors.first()->position().toPoint();
}

void WaylibCursor::setPos([[maybe_unused]] const QPoint &pos)
{

}

void WaylibCursor::addCursor(WCursor *cursor)
{
    cursors.append(cursor);
}

void WaylibCursor::removeCursor(WCursor *cursor)
{
    bool ok = cursors.removeOne(cursor);
    Q_ASSERT(ok);
}

WAYLIB_SERVER_END_NAMESPACE
