// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

namespace Waylib::Server {
class WServer;
class WBackend;
}

// Starts the headless backend and creates a wl_output global backed by a real
// headless wlr_output. The backend's output-added path creates the WOutput
// wrapper used by protocols that resolve wl_output resources server-side.
bool protocol_test_create_headless_output(Waylib::Server::WServer *server,
                                          int width = 1920,
                                          int height = 1080);

// Adds an output to an already-owned backend. The caller selects whether the
// backend must be started first; production Helper tests pass false because
// Helper::init() has already started it.
bool protocol_test_create_headless_output(Waylib::Server::WBackend *backend,
                                          bool startBackend,
                                          int width = 1920,
                                          int height = 1080);

// Advertises wl_shm with ARGB/XRGB buffers for clients that must map a real
// surface rather than only create a wl_surface proxy.
bool protocol_test_enable_shm(Waylib::Server::WServer *server);
