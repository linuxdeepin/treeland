// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "test-state.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "core/rootsurfacecontainer.h"
#include "session/session.h"
#include "session/sessionmanager.h"
#include "workspace/workspace.h"
#include "output/output.h"
#include <woutput.h>
#include "treelanduserconfig.hpp"
#include <wbackend.h>
#include <wserver.h>
#include <wsocket.h>
#include <wextworkspacemanagerv1.h>
#include <pwd.h>
#include <unistd.h>
#include <cstring>

namespace {
QPointer<Session> owners[2];
bool prepared = false;
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);
    owners[0] = server->sessionManager()->globalSession().get();
    const auto *account = getpwuid(getuid());
    if (account && owners[0] && owners[0]->username() == QString::fromLocal8Bit(account->pw_name))
        account = getpwnam("nobody");
    if (account)
        owners[1] = server->sessionManager()->ensureSession(
            QStringLiteral("protocol-ext-workspace"), QString::fromLocal8Bit(account->pw_name)).get();
    if (owners[1])
        owners[1]->socket()->setEnabled(true);
}

extern "C" bool protocol_test_ready(WaylandServer *server)
{
    if (!owners[0] || !owners[1])
        return true; // The client reports unavailable account fixtures as skipped.
    if (server->rootSurfaceContainer()->outputs().isEmpty()
        || !server->rootSurfaceContainer()->outputs().first()->output()->isEnabled())
        return false;
    for (const auto &owner : owners) {
        if (!owner->config()->isInitializeSucceeded())
            return false;
    }
    if (!prepared) {
        for (int i = 0; i < 2; ++i) {
            auto *workspace = owners[i]->workspace();
            owners[i]->config()->setMaxWorkspace(4);
            while (workspace->count() > 2)
                workspace->removeModel(workspace->count() - 1);
            while (workspace->count() < 2)
                workspace->createModel();
            workspace->setCurrentIndex(0);
            for (int j = 0; j < workspace->count(); ++j)
                workspace->modelAt(j)->setName(QStringLiteral("session-%1-workspace-%2").arg(i).arg(j));
        }
        prepared = true;
    }
    return true;
}

extern "C" void workspace_test_read(void *data)
{
    auto *state = static_cast<workspace_test_state *>(data);
    *state = {};
    if (!owners[0] || !owners[1])
        return;
    const auto socket = owners[1]->socket()->fullServerName().toUtf8();
    std::strncpy(state->background_socket, socket.constData(), sizeof(state->background_socket) - 1);
    WExtWorkspaceManagerV1 *managers[2] = {};
    for (auto *interface : WaylandServer::instance()->server()->interfaceList()) {
        auto *manager = dynamic_cast<WExtWorkspaceManagerV1 *>(interface);
        if (!manager)
            continue;
        for (int i = 0; i < 2; ++i) {
            if (interface->ownsSocket() == owners[i]->socket())
                managers[i] = manager;
        }
    }
    state->independent = managers[0] && managers[1] && managers[0]->handle()
        && managers[1]->handle() && managers[0]->handle() != managers[1]->handle();
    for (int i = 0; i < 2; ++i) {
        state->counts[i] = owners[i]->workspace()->count();
        state->current[i] = owners[i]->workspace()->currentIndex();
    }
}

extern "C" void workspace_test_remove_first(void *)
{
    owners[0]->workspace()->removeModel(0);
}

extern "C" void workspace_test_reorder(void *)
{
    auto *workspace = owners[0]->workspace();
    workspace->moveModelTo(workspace->current()->id(), 0);
}
