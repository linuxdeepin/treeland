// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "woutputitem.h"
#include "wcursor.h"

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WOutputCursor : public QObject
{
    friend class WOutputItem;
    friend class WOutputItemPrivate;

    Q_OBJECT
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WOutputItem* output READ output CONSTANT)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WCursor* cursor READ cursor CONSTANT)
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged FINAL)
    QML_ANONYMOUS

public:
    explicit WOutputCursor(WOutputItem *parent);

    inline WOutputItem *output() const {
        return static_cast<WOutputItem*>(parent());;
    }

    WCursor *cursor() const;
    bool visible() const;

Q_SIGNALS:
    void visibleChanged();

private:
    void setVisible(bool newVisible);

    QPointer<WCursor> m_cursor;
    QQuickItem *item = nullptr;
    bool m_visible = true;
};

WAYLIB_SERVER_END_NAMESPACE
