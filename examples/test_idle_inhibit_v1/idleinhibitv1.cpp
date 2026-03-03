// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "idleinhibitv1.h"

#define IDLE_INHIBIT_MANAGER_V1_VERSION 1

IdleInhibitManagerV1::IdleInhibitManagerV1()
    : QWaylandClientExtensionTemplate<IdleInhibitManagerV1>(IDLE_INHIBIT_MANAGER_V1_VERSION)
{
}

IdleInhibitManagerV1::~IdleInhibitManagerV1()
{
    if (isInitialized()) {
        destroy();
    }
}

void IdleInhibitManagerV1::instantiate()
{
    initialize();
}

std::unique_ptr<IdleInhibitorV1> IdleInhibitManagerV1::createInhibitor(struct ::wl_surface *surface)
{
    if (!isInitialized() || !surface) {
        return nullptr;
    }
    auto inhibitor = create_inhibitor(surface);
    if (!inhibitor) {
        return nullptr;
    }
    return std::make_unique<IdleInhibitorV1>(inhibitor);
}

IdleInhibitorV1::IdleInhibitorV1(struct ::zwp_idle_inhibitor_v1 *id)
    : QtWayland::zwp_idle_inhibitor_v1(id)
{
}

IdleInhibitorV1::~IdleInhibitorV1()
{
    destroy();
}
