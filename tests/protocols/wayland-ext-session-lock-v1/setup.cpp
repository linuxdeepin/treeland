// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "server-bridge.h"
#include "seat/helper.h"
#include <wbackend.h>

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);
}
