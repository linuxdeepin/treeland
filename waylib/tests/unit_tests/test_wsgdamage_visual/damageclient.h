// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef DAMAGECLIENT_H
#define DAMAGECLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

struct DamageClient;

enum {
    DAMAGE_CLIENT_WIDTH = 64,
    DAMAGE_CLIENT_HEIGHT = 64
};

/* Takes ownership of `fd`. Call damage_client_run() on a worker thread. */
struct DamageClient *damage_client_create(int fd);
void damage_client_run(struct DamageClient *client);
int damage_client_is_mapped(const struct DamageClient *client);
const char *damage_client_error(const struct DamageClient *client);
int damage_client_commit_damage(struct DamageClient *client, int x, int y, int w, int h);
int damage_client_commit_damage_only(struct DamageClient *client, int x, int y, int w, int h);
void damage_client_stop(struct DamageClient *client);
void damage_client_destroy(struct DamageClient *client);

#ifdef __cplusplus
}
#endif

#endif
