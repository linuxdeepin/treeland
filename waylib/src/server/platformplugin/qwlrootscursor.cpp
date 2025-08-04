// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwlrootscursor.h"
#include "qwlrootswindow.h"
#include "wcursor.h"
#include "types.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

QWlrootsCursor::QWlrootsCursor()
    : QPlatformCursor()
{

}

#ifndef QT_NO_CURSOR
void QWlrootsCursor::changeCursor(QCursor *windowCursor, QWindow *window)
{
    if (!window || !QW::RenderWindow::check(window))
        return;

    auto cursor = static_cast<QWlrootsRenderWindow*>(window->handle())->lastActiveCursor;
    if (!cursor)
        return;

    cursor->setCursor(windowCursor ? *windowCursor : WCursor::defaultCursor());
}

void QWlrootsCursor::setOverrideCursor(const QCursor &cursor)
{
    Q_UNUSED(cursor);
    return;
}

void QWlrootsCursor::clearOverrideCursor()
{

}
#endif

QPoint QWlrootsCursor::pos() const
{
    return cursors.isEmpty() ? QPoint() : cursors.first()->position().toPoint();
}

void QWlrootsCursor::setPos([[maybe_unused]] const QPoint &pos)
{

}

void QWlrootsCursor::addCursor(WCursor *cursor)
{
    cursors.append(cursor);
}

void QWlrootsCursor::removeCursor(WCursor *cursor)
{
    bool ok = cursors.removeOne(cursor);
    Q_ASSERT(ok);
}

WAYLIB_SERVER_END_NAMESPACE
