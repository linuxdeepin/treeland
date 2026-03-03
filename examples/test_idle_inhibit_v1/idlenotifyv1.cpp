// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "idlenotifyv1.h"

#define EXT_IDLE_NOTIFIER_V1_VERSION 1

IdleNotifierV1::IdleNotifierV1()
    : QWaylandClientExtensionTemplate<IdleNotifierV1>(EXT_IDLE_NOTIFIER_V1_VERSION)
{
}

IdleNotifierV1::~IdleNotifierV1()
{
    if (isInitialized()) {
        destroy();
    }
}

void IdleNotifierV1::instantiate()
{
    initialize();
}

std::unique_ptr<IdleNotificationV1> IdleNotifierV1::getIdleNotification(uint32_t timeout,
                                                                        struct ::wl_seat *seat)
{
    if (!isInitialized() || !seat) {
        return nullptr;
    }
    auto notification = get_idle_notification(timeout, seat);
    if (!notification) {
        return nullptr;
    }
    return std::make_unique<IdleNotificationV1>(notification);
}

IdleNotificationV1::IdleNotificationV1(struct ::ext_idle_notification_v1 *id)
    : QtWayland::ext_idle_notification_v1(id)
{
}

IdleNotificationV1::~IdleNotificationV1()
{
    destroy();
}

void IdleNotificationV1::ext_idle_notification_v1_idled()
{
    Q_EMIT idled();
}

void IdleNotificationV1::ext_idle_notification_v1_resumed()
{
    Q_EMIT resumed();
}
