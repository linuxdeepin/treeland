// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

enum {
    OUTPUT_MANAGEMENT_MAX_OUTPUTS = 4,
    OUTPUT_MANAGEMENT_MAX_NAME = 64,
};

// One fixture output, as the production objects report it. Outputs are indexed
// in the order the compositor registered them.
struct output_management_output_state {
    char name[OUTPUT_MANAGEMENT_MAX_NAME];
    int enabled;
    int x;
    int y;
    int transform;
    int scale_milli;
    int pending_state;
    int pending_transform;
    int pending_scale_milli;
};

struct output_management_server_state {
    int output_count;
    struct output_management_output_state outputs[OUTPUT_MANAGEMENT_MAX_OUTPUTS];
    int request_count;
    int last_request_test;
    int last_request_transform;
    int last_request_scale_milli;
};

// Asks the fixture to enable or disable one output by name, like the compositor
// side of a display panel turning a screen off.
struct output_management_output_request {
    char name[OUTPUT_MANAGEMENT_MAX_NAME];
    int enabled;
};
