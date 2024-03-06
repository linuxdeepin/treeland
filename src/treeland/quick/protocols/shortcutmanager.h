// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <wquickwaylandserver.h>

#include "helper.h"

class ShortcutManagerV1Private;
class ShortcutManagerV1 : public Waylib::Server::WQuickWaylandServerInterface , public WObject{
    Q_OBJECT
    QML_NAMED_ELEMENT(ShortcutManager)
    W_DECLARE_PRIVATE(ShortcutManagerV1)
    Q_PROPERTY(Helper *helper READ helper WRITE setHelper)

public:
    explicit ShortcutManagerV1(QObject *parent = nullptr);

protected:
    void create() override;

private:
    void setHelper(Helper *helper);
    Helper *helper();
};
