// Copyright (C) 2024 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>
#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickScreenCopyManagerPrivate;
class WAYLIB_SERVER_EXPORT WQuickScreenCopyManager : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickScreenCopyManager)
    QML_NAMED_ELEMENT(ScreenCopyManager)

public:
    explicit WQuickScreenCopyManager(QObject *parent = nullptr);

private:
    void create() override;
};

WAYLIB_SERVER_END_NAMESPACE
