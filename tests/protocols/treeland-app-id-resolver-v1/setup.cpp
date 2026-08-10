// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/app-id-resolver/appidresolver.h"

#include <wserver.h>

#include <QString>

#include <sys/syscall.h>
#include <unistd.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
AppIdResolverManager *g_manager = nullptr;
}

extern "C" {

/*
 * Resolve state shared with the pure-C client test (mirrored struct in
 * treeland-app-id-resolver-v1.c). All writes happen on the server (Qt main)
 * thread; the client reads g_app_id_resolver_snapshot, which is copied on the
 * server thread inside protocol_test_invoke_server() so the semaphore provides
 * the cross-thread synchronization.
 */
struct app_id_resolver_test_state {
    int resolve_started; /* AppIdResolverManager::resolvePidfd accepted the request */
    int resolve_matched; /* resolve callback received the expected app id */
    int resolve_empty;   /* resolve callback received an empty app id */
};

struct app_id_resolver_test_state g_app_id_resolver_state;
struct app_id_resolver_test_state g_app_id_resolver_snapshot;

/* Ask the server to resolve the test process's own pidfd. */
void server_start_resolve(void *)
{
    g_app_id_resolver_state.resolve_started = 0;
    g_app_id_resolver_state.resolve_matched = 0;
    g_app_id_resolver_state.resolve_empty = 0;

    if (!g_manager)
        return;

    const int pidfd = static_cast<int>(syscall(SYS_pidfd_open, static_cast<long>(getpid()), 0));
    if (pidfd < 0)
        return;

    const bool started = g_manager->resolvePidfd(pidfd, [](const QString &appId) {
        g_app_id_resolver_state.resolve_matched =
            (appId == QStringLiteral("org.deepin.dde.test-app")) ? 1 : 0;
        g_app_id_resolver_state.resolve_empty = appId.isEmpty() ? 1 : 0;
    });
    close(pidfd);
    g_app_id_resolver_state.resolve_started = started ? 1 : 0;
}

void server_snapshot_state(void *)
{
    g_app_id_resolver_snapshot = g_app_id_resolver_state;
}

} // extern "C"

void protocol_test_setup(WServer *server)
{
    g_manager = server->attach<AppIdResolverManager>();
}
