// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

struct output_power_server_state {
    int output_count;
    int enabled;
};

struct output_power_window_state {
    int window_count;
    int outputs;
    int enabled_outputs;
};
