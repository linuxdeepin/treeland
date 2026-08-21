// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/capture/capture.h"
#include "server-bridge.h"

#include <woutputrenderwindow.h>
#include <wserver.h>

WAYLIB_SERVER_USE_NAMESPACE

void protocol_test_setup(Helper *helper)
{
    Q_ASSERT(find_server_interface<CaptureManagerV1>(helper));
}
