// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include "qwayland-ext-idle-notify-v1.h"

#include <QObject>
#include <QWaylandClientExtension>

class IdleNotificationV1;

class IdleNotifierV1
    : public QWaylandClientExtensionTemplate<IdleNotifierV1>
    , public QtWayland::ext_idle_notifier_v1
{
    Q_OBJECT
public:
    IdleNotifierV1();
    ~IdleNotifierV1();

    void instantiate();

    std::unique_ptr<IdleNotificationV1> getIdleNotification(uint32_t timeout,
                                                            struct ::wl_seat *seat);
};

class IdleNotificationV1
    : public QObject
    , public QtWayland::ext_idle_notification_v1
{
    Q_OBJECT
public:
    explicit IdleNotificationV1(struct ::ext_idle_notification_v1 *id);
    ~IdleNotificationV1();

Q_SIGNALS:
    void idled();
    void resumed();

protected:
    void ext_idle_notification_v1_idled() override;
    void ext_idle_notification_v1_resumed() override;
};
