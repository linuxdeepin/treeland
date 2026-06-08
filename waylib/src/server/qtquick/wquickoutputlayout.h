// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <woutputlayout.h>

#include <QObject>
#include <QQmlEngine>

Q_MOC_INCLUDE("woutputitem.h")

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputItem;
class WQuickOutputLayoutPrivate;
class WAYLIB_SERVER_EXPORT WQuickOutputLayout : public WOutputLayout
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickOutputLayout)
    QML_NAMED_ELEMENT(OutputLayout)
    Q_PROPERTY(QList<WOutputItem*> outputs READ outputs NOTIFY outputsChanged)

public:
    explicit WQuickOutputLayout(WServer *server);

    const QList<WOutputItem *> &outputs() const;

public Q_SLOTS:
    void add(WOutputItem *output);
    void remove(WOutputItem *output);

Q_SIGNALS:
    void outputsChanged();
    void maybeLayoutChanged();

private:
    using WOutputLayout::add;
    using WOutputLayout::remove;
    using WOutputLayout::move;
};

WAYLIB_SERVER_END_NAMESPACE
