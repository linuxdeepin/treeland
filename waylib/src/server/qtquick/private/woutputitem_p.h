// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "woutputitem.h"

#include <qwoutput.h>
#include <qwtexture.h>

#include <QQmlEngine>
#include <QQuickTextureFactory>
#include <private/qquickitem_p.h>

Q_MOC_INCLUDE(<wcursor.h>)

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WOutputItemAttached : public QObject
{
    friend class WOutputItem;
    Q_OBJECT
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WOutputItem* item READ item NOTIFY itemChanged FINAL)
    QML_ANONYMOUS

public:
    explicit WOutputItemAttached(QObject *parent = nullptr);

    WOutputItem *item() const;

Q_SIGNALS:
    void itemChanged();

private:
    void setItem(WOutputItem *positioner);

private:
    WOutputItem *m_positioner = nullptr;
};

class WCursor;
class Q_DECL_HIDDEN WOutputCursor : public QObject
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
