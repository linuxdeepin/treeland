// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"

#include <qwglobal.h>

#include <QCursor>
#include <qpa/qplatformcursor.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WCursor;
class Q_DECL_HIDDEN QWlrootsCursor : public QPlatformCursor
{
public:
    explicit QWlrootsCursor();

#ifndef QT_NO_CURSOR
    void changeCursor(QCursor *windowCursor, QWindow *window) override;
    void setOverrideCursor(const QCursor &) override;
    void clearOverrideCursor() override;
#endif // QT_NO_CURSOR
    QPoint pos() const override;
    void setPos(const QPoint &pos) override;

    void addCursor(WCursor *cursor);
    void removeCursor(WCursor *cursor);

private:
    friend class WOutput;

    QList<WCursor*> cursors;
};

WAYLIB_SERVER_END_NAMESPACE
