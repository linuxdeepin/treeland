// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "qwayland-idle-inhibit-unstable-v1.h"

#include <QObject>
#include <QWaylandClientExtension>
#include <QWindow>

class IdleInhibitorV1;

class IdleInhibitManagerV1
    : public QWaylandClientExtensionTemplate<IdleInhibitManagerV1>
    , public QtWayland::zwp_idle_inhibit_manager_v1
{
    Q_OBJECT
public:
    IdleInhibitManagerV1();
    ~IdleInhibitManagerV1();

    void instantiate();

    std::unique_ptr<IdleInhibitorV1> createInhibitor(struct ::wl_surface *surface);
};

class IdleInhibitorV1
    : public QObject
    , public QtWayland::zwp_idle_inhibitor_v1
{
    Q_OBJECT
public:
    explicit IdleInhibitorV1(struct ::zwp_idle_inhibitor_v1 *id);
    ~IdleInhibitorV1();
};
