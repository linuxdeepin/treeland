// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "protocol-test-client.h"
#include "protocol-test-xdg-client.h"

struct wine_ws_state {
    int wrapper_created;
    int minimized;
    int attention;
    int visible;  // E-level: QQuickItem visibility (reflects isMinimized() → setVisible(false))
};

int protocol_test_run(const char *socket_name);

#ifdef __cplusplus
}
#endif
