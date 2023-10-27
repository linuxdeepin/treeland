// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR
// GPL-3.0-only

#pragma once

#include <cassert>

#include "socket-server-protocol.h"

#define TREELAND_SOCKET_MANAGER_V1_VERSION 1

namespace Waylib::Server {
class WServer;
}

class SocketManager;
class TreeLandHelper;

struct treeland_socket_manager_v1 {
  struct wl_global *global;

  struct {
    struct wl_signal new_user_socket;
    struct wl_signal destroy;
  } events;

  void *data;

  struct wl_list contexts; // link to treeland_socket_context_v1.link

  struct wl_listener display_destroy;

  SocketManager *manager = nullptr;
};

struct treeland_socket_context_v1_state {
  char *username;
  int32_t fd;
};

struct treeland_socket_manager_v1 *socket_manager_from_resource(struct wl_resource *resource);
void socket_manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id);
void socket_manager_handle_display_destroy(struct wl_listener *listener, void *data);