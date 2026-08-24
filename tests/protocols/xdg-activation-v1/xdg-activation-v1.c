// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "xdg-activation-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>

struct token_info {
    int done;
    char token[256];
};

static void handle_done(void *data, struct xdg_activation_token_v1 *token,
                        const char *t)
{
    (void)token;
    struct token_info *info = data;
    info->done = 1;
    snprintf(info->token, sizeof(info->token), "%s", t);
}

static const struct xdg_activation_token_v1_listener token_listener = {
    .done = handle_done,
};

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct xdg_activation_v1 *activation =
        client_bind(&conn, "xdg_activation_v1", &xdg_activation_v1_interface, 1);
    if (!activation) {
        fprintf(stderr, "xdg-activation: failed to bind\n");
        client_disconnect(&conn);
        return 1;
    }

    /* Positive: request a token and verify the done event carries a string. */
    struct xdg_activation_token_v1 *token =
        xdg_activation_v1_get_activation_token(activation);
    if (!token) {
        fprintf(stderr, "xdg-activation: get_activation_token returned null\n");
        client_disconnect(&conn);
        return 1;
    }

    struct token_info info = {0};
    xdg_activation_token_v1_add_listener(token, &token_listener, &info);
    xdg_activation_token_v1_set_app_id(token, "test.app");
    xdg_activation_token_v1_commit(token);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "xdg-activation: roundtrip after commit failed\n");
        client_disconnect(&conn);
        return 1;
    }

    if (!info.done || strlen(info.token) == 0) {
        fprintf(stderr, "xdg-activation: did not receive a valid token\n");
        client_disconnect(&conn);
        return 1;
    }

    /* Negative: committing a second time must raise already_used. */
    xdg_activation_token_v1_commit(token);
    if (wl_display_roundtrip(conn.display) >= 0) {
        fprintf(stderr, "xdg-activation: duplicate commit did not raise an error\n");
        client_disconnect(&conn);
        return 1;
    }

    /* Expected error raised; connection is fatal. */
    return 0;
}
