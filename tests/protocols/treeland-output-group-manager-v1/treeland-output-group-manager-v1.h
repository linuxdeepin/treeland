// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int protocol_test_run(const char *socket_name);

#include "client-connection.h"

#define TEST_MSG_MAX 256

struct test_result {
    const char *name;
    int failed;
    char message[TEST_MSG_MAX];
};

enum {
    OUTPUT_GROUP_TEST_MAX_NAME = 64,
    OUTPUT_GROUP_TEST_MAX_OUTPUTS = 4,
    OUTPUT_GROUP_TEST_MAX_GROUPS = 8,
    OUTPUT_GROUP_TEST_MAX_TEXT = 256,
};

// Asks the fixture to enable or disable one output by uuid.
struct output_group_output_state {
    char uuid[OUTPUT_GROUP_TEST_MAX_NAME];
    int enabled;
};

// Snapshot of the compositor state, read on the compositor thread. Outputs are
// indexed in the order the compositor registered them.
struct output_group_server_state {
    int output_count;
    int group_count;
    int is_enabled[OUTPUT_GROUP_TEST_MAX_OUTPUTS];
    int is_source[OUTPUT_GROUP_TEST_MAX_OUTPUTS];
    int is_copy[OUTPUT_GROUP_TEST_MAX_OUTPUTS];
    int proxy_index[OUTPUT_GROUP_TEST_MAX_OUTPUTS];
    char uuids[OUTPUT_GROUP_TEST_MAX_OUTPUTS][OUTPUT_GROUP_TEST_MAX_NAME];
    char names[OUTPUT_GROUP_TEST_MAX_OUTPUTS][OUTPUT_GROUP_TEST_MAX_NAME];
    char group_names[OUTPUT_GROUP_TEST_MAX_TEXT];
    char group_sources[OUTPUT_GROUP_TEST_MAX_TEXT];
};

#ifdef __cplusplus
}
#endif
