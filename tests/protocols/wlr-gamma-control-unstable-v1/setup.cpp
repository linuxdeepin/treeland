// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-3.0-only

#include "server-bridge.h"
#include "seat/helper.h"

void protocol_test_setup(Helper *helper)
{
#ifndef TREELAND_PROTOCOL_EXPECT_DRM_GAMMA
    // wlroots headless outputs advertise gamma size zero. That is the CI
    // capability boundary this protocol test deliberately observes.
    add_headless_output(helper->backend(), false);
#else
    // The opt-in runner supplies WLR_BACKENDS=drm. Do not add a headless
    // output: only the physical DRM output can advertise a hardware LUT.
    (void)helper;
#endif
}
