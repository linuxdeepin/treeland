// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwayland-treeland-dde-shell-v1.h"

#include <qwaylandclientextension.h>

#include <qqmlintegration.h>
#include <qtmetamacros.h>

class DDEShell
    : public QWaylandClientExtensionTemplate<DDEShell>
    , public QtWayland::treeland_dde_shell_manager_v1
{
    Q_OBJECT
public:
    explicit DDEShell()
        : QWaylandClientExtensionTemplate<DDEShell>(1)
    {
    }
};

class Multitaskview
    : public QWaylandClientExtensionTemplate<Multitaskview>
    , public QtWayland::treeland_multitaskview_v1
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged FINAL)

    bool ready()
    {
        return object() != nullptr;
    }

Q_SIGNALS:
    void readyChanged();

public:
    explicit Multitaskview();
    ~Multitaskview();
    Q_INVOKABLE void toggle();

private:
    DDEShell *m_manager;
};
