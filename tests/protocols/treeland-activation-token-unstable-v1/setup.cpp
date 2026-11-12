// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "activation-token-fixture.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "modules/activation/activationmanagerinterfacev1.h"
#include "session/session.h"
#include "session/sessionmanager.h"

#include <wbackend.h>
#include <wsocket.h>

#include <pwd.h>
#include <unistd.h>

namespace {
bool g_activate_requested = false;
int g_disposition = 0;
std::shared_ptr<Session> g_session_b;
}

// A second real Session (another user) for the cross-session assertions. Same
// account selection as the session isolation fixture.
static std::shared_ptr<Session> createSecondSession(WaylandServer *server)
{
    auto sessionA = server->sessionManager()->globalSession();
    const auto *account = getpwuid(getuid());
    if (account && sessionA && sessionA->username() == QString::fromLocal8Bit(account->pw_name))
        account = getpwnam("nobody");
    if (!account || !sessionA)
        return {};
    auto sessionB = server->sessionManager()->ensureSession(
        QStringLiteral("protocol-activation-token"), QString::fromLocal8Bit(account->pw_name));
    if (!sessionB || sessionB == sessionA)
        return {};
    // Keep the background session dispatchable for the client connections.
    sessionB->socket()->setEnabled(true);
    return sessionB;
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);
    g_session_b = createSecondSession(server);

    // One activation manager per Session, so subscribe after every Session
    // exists: the readback must work whichever session the client belongs to.
    for (auto *candidate : native_server(server)->interfaceList()) {
        auto *activation = dynamic_cast<ActivationManagerInterfaceV1 *>(candidate);
        if (!activation)
            continue;
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
    state->second_session = g_session_b ? 1 : 0;
    if (g_session_b)
        qstrncpy(state->second_socket,
                 g_session_b->socket()->fullServerName().toUtf8().constData(),
                 sizeof(state->second_socket));
}
