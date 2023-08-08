// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <QQmlEngine>

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WQmlHelper : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(WaylibHelper)
    QML_SINGLETON

public:
    explicit WQmlHelper(QObject *parent = nullptr);

public Q_SLOTS:
    void itemStackToTop(QQuickItem *item);
};

WAYLIB_SERVER_END_NAMESPACE
