// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "activation-token-fixture.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "modules/activation/activationmanagerinterfacev1.h"

#include <wbackend.h>

namespace {
bool g_activate_requested = false;
int g_disposition = 0;
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);

    auto *activation = find_server_interface<ActivationManagerInterfaceV1>(server);
    if (activation) {
        QObject::connect(activation,
                         &ActivationManagerInterfaceV1::activateRequested,
                         server,
                         [](const QString &,
                            ActivationManagerInterfaceV1::TokenDisposition disposition,
                            WAYLIB_SERVER_NAMESPACE::WSurface *,
                            WAYLIB_SERVER_NAMESPACE::WSeat *,
                            WAYLIB_SERVER_NAMESPACE::WSurface *) {
                             g_activate_requested = true;
                             g_disposition = static_cast<int>(disposition);
                         });
    }
}

extern "C" void activation_token_read_server_state(void *data)
{
    auto *state = static_cast<struct activation_token_server_state *>(data);
    state->activate_requested = g_activate_requested ? 1 : 0;
    state->disposition = g_disposition;
}
