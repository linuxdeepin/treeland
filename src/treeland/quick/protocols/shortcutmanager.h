// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <cassert>

#include <QObject>
#include <QQmlEngine>
#include <wquickwaylandserver.h>

#include "shortcut-server-protocol.h"

#include "treelandhelper.h"

class ShortcutManager : public Waylib::Server::WQuickWaylandServerInterface {
    Q_OBJECT
    Q_PROPERTY(TreeLandHelper *helper READ helper WRITE setHelper)

    QML_ELEMENT

public:
    explicit ShortcutManager(QObject *parent = nullptr);

    ztreeland_shortcut_manager_v1 *impl();

    TreeLandHelper *helper() const {
        return m_helper;
    }

protected:
    void create() override;

private:
    void setHelper(TreeLandHelper *helper);

private:
    ztreeland_shortcut_manager_v1 *m_impl;
    TreeLandHelper *m_helper;
};
