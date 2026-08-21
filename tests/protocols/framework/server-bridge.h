// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

namespace Waylib::Server {
class WBackend;
}

class Helper;

Waylib::Server::WServer *server_for_helper(Helper *helper);

template<typename Interface>
Interface *find_server_interface(Helper *helper)
{
    auto *server = server_for_helper(helper);
    if (!server)
        return nullptr;
    for (auto *candidate : server->interfaceList()) {
        if (auto *interface = dynamic_cast<Interface *>(candidate))
            return interface;
    }
    return nullptr;
}

// Starts the headless backend and adds an output to it. The backend's
// output-added path creates the WOutput wrapper used by protocols that resolve
// wl_output resources server-side.
bool add_headless_output(Waylib::Server::WServer *server,
                         int width = 1920,
                         int height = 1080);

// Adds an output to an already-owned backend. The caller selects whether the
// backend must be started first; production Helper tests pass false because
// Helper::init() has already started it.
bool add_headless_output(Waylib::Server::WBackend *backend,
                         bool startBackend,
                         int width = 1920,
                         int height = 1080);
