// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "woutputitem.h"

#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputItemAttached : public QObject
{
    friend class WOutputItem;
    Q_OBJECT
    Q_PROPERTY(WOutputItem* item READ item NOTIFY itemChanged FINAL)
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

WAYLIB_SERVER_END_NAMESPACE
