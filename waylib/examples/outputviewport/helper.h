// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WOutput>

#include <QObject>
#include <QQmlEngine>

WAYLIB_SERVER_USE_NAMESPACE

class Helper : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    Q_INVOKABLE void enableOutput(WOutput *output);
};
