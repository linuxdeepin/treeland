// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

struct output_management_server_state {
    int output_count;
    int x;
    int y;
    int enabled;
    int transform;
    int scale_milli;
    int pending_state;
    int pending_transform;
    int pending_scale_milli;
    int request_count;
    int last_request_test;
    int last_request_transform;
    int last_request_scale_milli;
};
