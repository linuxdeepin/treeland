// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

namespace Waylib::Server {
class WBackend;
class WOutput;
}

class WaylandServer;

Waylib::Server::WServer *native_server(WaylandServer *server);

// Adds a virtual wlroots pointer so the production seat advertises pointer
// capability and clients can create wl_pointer resources.
void attach_test_pointer_device(WaylandServer *server);

template<typename Interface>
Interface *find_server_interface(WaylandServer *server)
{
    auto *nativeServer = native_server(server);
    if (!nativeServer)
        return nullptr;
    for (auto *candidate : nativeServer->interfaceList()) {
        if (auto *interface = dynamic_cast<Interface *>(candidate))
            return interface;
    }
    return nullptr;
}

// Starts the headless backend and adds an output to it. The backend's
// output-added path creates the WOutput wrapper used by protocols that resolve
// wl_output resources server-side.
Waylib::Server::WOutput *add_headless_output(Waylib::Server::WServer *server,
                         int width = 1920,
                         int height = 1080);

// Adds an output to an already-owned backend. The caller selects whether the
// backend must be started first; production WaylandServer tests pass false because
// WaylandServer::init() has already started it.
Waylib::Server::WOutput *add_headless_output(Waylib::Server::WBackend *backend,
                         bool startBackend,
                         int width = 1920,
                         int height = 1080);
