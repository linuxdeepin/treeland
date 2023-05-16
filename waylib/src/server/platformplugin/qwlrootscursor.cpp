// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwlrootscursor.h"
#include "wcursor.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

QWlrootsCursor::QWlrootsCursor(WCursor *cursor)
    : QPlatformCursor()
    , m_cursor(cursor)
{

}

#ifndef QT_NO_CURSOR
void QWlrootsCursor::changeCursor(QCursor *windowCursor, QWindow *)
{
    if (windowCursor)
        m_cursor->setCursor(*windowCursor);
    else
        m_cursor->setCursor(WCursor::defaultCursor()); // default type
}

void QWlrootsCursor::setOverrideCursor(const QCursor &cursor)
{
    m_cursor->setCursor(cursor);
}

void QWlrootsCursor::clearOverrideCursor()
{
    m_cursor->reset(); // default type
}
#endif

QPoint QWlrootsCursor::pos() const
{
    return m_cursor->position().toPoint();
}

void QWlrootsCursor::setPos(const QPoint &pos)
{
    return m_cursor->setPosition(pos);
}

WAYLIB_SERVER_END_NAMESPACE
