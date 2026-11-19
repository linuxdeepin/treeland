// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct socket_endpoint {
    int fd;
    char name[108];
};

struct socket_lifecycle_state {
    struct socket_endpoint original;
    struct socket_endpoint replacement;
    char descendant[108];
    int ready;
    int enable_synced;
    int released;
};

enum socket_owner {
    FIRST_OWNER,
    SECOND_OWNER,
};

void socket_lifecycle_prepare(void *data);
void socket_lifecycle_check_enable_sync(void *data);
int socket_lifecycle_register(enum socket_owner owner, int fd);
int socket_lifecycle_disconnect_owner(enum socket_owner owner);
void socket_lifecycle_add_descendant(void *data);
void socket_lifecycle_logout(void *data);
void socket_lifecycle_cleanup(void *data);

#ifdef __cplusplus
}
#endif
