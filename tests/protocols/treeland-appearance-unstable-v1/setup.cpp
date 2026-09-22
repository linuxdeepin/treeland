// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/appearance/appearanceinterfacev1.h"
#include "modules/appearance/appearancemanagerinterfacev1.h"
#include "server-bridge.h"

void protocol_test_setup(Helper *helper)
{
    Q_ASSERT(find_server_interface<AppearanceInterfaceV1>(helper));
    Q_ASSERT(find_server_interface<AppearanceManagerInterfaceV1>(helper));
}
