// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QQuickItem>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WEventJunkman : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(EventJunkman)

public:
    explicit WEventJunkman(QQuickItem *parent = nullptr);

private:
    bool event(QEvent *event) override;
};

WAYLIB_SERVER_END_NAMESPACE
