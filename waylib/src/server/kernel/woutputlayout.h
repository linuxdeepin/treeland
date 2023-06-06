// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <qwoutputlayout.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WOutputLayoutPrivate;
class WAYLIB_SERVER_EXPORT WOutputLayout : public QW_NAMESPACE::QWOutputLayout, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WOutputLayout)

public:
    explicit WOutputLayout(QObject *parent = nullptr);

    QList<WOutput*> outputs() const;

    void add(WOutput *output, const QPoint &pos);
    void move(WOutput *output, const QPoint &pos);
    void remove(WOutput *output);

Q_SIGNALS:
    void outputAdded(WOutput *output);
    void outputRemoved(WOutput *output);

protected:
    WOutputLayout(WOutputLayoutPrivate &dd, QObject *parent = nullptr);

protected:
    using QW_NAMESPACE::QWOutputLayout::add;
    using QW_NAMESPACE::QWOutputLayout::addAuto;
    using QW_NAMESPACE::QWOutputLayout::move;
    using QW_NAMESPACE::QWOutputLayout::remove;
};

WAYLIB_SERVER_END_NAMESPACE
