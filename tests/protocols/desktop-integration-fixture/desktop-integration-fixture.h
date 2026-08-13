// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
/*
 * C ABI for the production desktop integration fixture.
 */
#pragma once

#include "protocol-test-client.h"

struct desktop_fixture_state {
    int output_ready;
    int wrapper_created;
    int wrapper_in_workspace;
};
