// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <woutputlayout.h>

#include <QObject>
#include <QQmlEngine>

Q_MOC_INCLUDE("woutputviewport.h")

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputViewport;
class WQuickOutputLayoutPrivate;
class WAYLIB_SERVER_EXPORT WQuickOutputLayout : public WOutputLayout
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickOutputLayout)
    QML_NAMED_ELEMENT(OutputLayout)
    Q_PROPERTY(QList<WAYLIB_SERVER_NAMESPACE::WOutputViewport*> outputs READ outputs NOTIFY outputsChanged)

public:
    explicit WQuickOutputLayout(QObject *parent = nullptr);

    QList<WOutputViewport*> outputs() const;

public Q_SLOTS:
    void add(WAYLIB_SERVER_NAMESPACE::WOutputViewport *output);
    void remove(WAYLIB_SERVER_NAMESPACE::WOutputViewport *output);

Q_SIGNALS:
    void outputsChanged();

private:
    using WOutputLayout::add;
    using WOutputLayout::remove;
    using WOutputLayout::move;

    W_PRIVATE_SLOT(void onOutputLayoutChanged())
};

WAYLIB_SERVER_END_NAMESPACE
