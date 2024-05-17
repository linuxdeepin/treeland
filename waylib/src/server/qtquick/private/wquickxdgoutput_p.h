// Copyright (C) 2024 Lu YaNing <luyaning@uniontech.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>
#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputLayout;
class WQuickXdgOutputManagerPrivate;
class WAYLIB_SERVER_EXPORT WQuickXdgOutputManager : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    Q_PROPERTY(WOutputLayout* layout READ layout WRITE setLayout REQUIRED)
    W_DECLARE_PRIVATE(WQuickXdgOutputManager)
    QML_NAMED_ELEMENT(XdgOutputManager)

public:
    explicit WQuickXdgOutputManager(QObject *parent = nullptr);

    void setLayout(WOutputLayout *layout);
    WOutputLayout *layout() const;

private:
    void create() override;
};

WAYLIB_SERVER_END_NAMESPACE
