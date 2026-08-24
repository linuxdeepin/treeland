// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "client-connection.h"
#include "security-context-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

int protocol_test_run(const char *socket_name)
{
    struct client_connection conn;
    if (!client_connect(&conn, socket_name))
        return 1;

    struct wp_security_context_manager_v1 *manager =
        client_bind(&conn, "wp_security_context_manager_v1",
                    &wp_security_context_manager_v1_interface, 1);
    if (!manager) {
        fprintf(stderr, "security-context: failed to bind\n");
        client_disconnect(&conn);
        return 1;
    }

    /* Create a listening UNIX socket for listen_fd. */
    int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        fprintf(stderr, "security-context: socket() failed\n");
        client_disconnect(&conn);
        return 1;
    }
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path),
             "/tmp/treeland-sec-ctx-%d", (int)getpid());
    unlink(addr.sun_path);
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
        listen(listen_fd, 1) < 0) {
        fprintf(stderr, "security-context: bind/listen failed\n");
        close(listen_fd);
        unlink(addr.sun_path);
        client_disconnect(&conn);
        return 1;
    }
    unlink(addr.sun_path);

    /* Create a pipe for close_fd. */
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        fprintf(stderr, "security-context: pipe() failed\n");
        close(listen_fd);
        client_disconnect(&conn);
        return 1;
    }

    struct wp_security_context_v1 *ctx =
        wp_security_context_manager_v1_create_listener(manager, listen_fd,
                                                        pipefd[1]);
    if (!ctx) {
        fprintf(stderr, "security-context: create_listener returned null\n");
        close(listen_fd);
        close(pipefd[0]);
        close(pipefd[1]);
        client_disconnect(&conn);
        return 1;
    }

    wp_security_context_v1_set_app_id(ctx, "test.security.context");
    wp_security_context_v1_commit(ctx);
    if (wl_display_roundtrip(conn.display) < 0) {
        fprintf(stderr, "security-context: roundtrip after commit failed\n");
        /* connection may be dead */
        close(listen_fd);
        close(pipefd[0]);
        close(pipefd[1]);
        return 1;
    }

    wp_security_context_v1_destroy(ctx);
    close(listen_fd);
    close(pipefd[0]);
    close(pipefd[1]);
    client_disconnect(&conn);
    return 0;
}
