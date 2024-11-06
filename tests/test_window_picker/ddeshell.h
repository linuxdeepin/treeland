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

class WindowPickerClient
    : public QWaylandClientExtensionTemplate<WindowPickerClient>
    , public QtWayland::treeland_window_picker_v1
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged FINAL)
    Q_PROPERTY(int pid READ pid NOTIFY pidChanged FINAL)

    int pid() const
    {
        return m_pid;
    }

    bool ready()
    {
        return object() != nullptr;
    }

Q_SIGNALS:
    void readyChanged();
    void pidChanged();

public:
    explicit WindowPickerClient();
    ~WindowPickerClient();
    Q_INVOKABLE void pick();

private:
    DDEShell *m_manager;
    int m_pid{ -1 };

    // treeland_window_picker_v1 interface
protected:
    void treeland_window_picker_v1_window(int32_t pid) override;
};
