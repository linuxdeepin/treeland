// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "session-protocol-lifecycle.h"
#include "core/waylandserver.h"
#include "session/sessionmanager.h"
#include "session/session.h"
#include "workspace/workspace.h"
#include "surface/surfacewrapper.h"
#include "server-bridge.h"

#include <wserver.h>
#include <wsocket.h>
#include <wlr_all.h>

#include <cstring>
#include <list>
#include <pwd.h>
#include <unistd.h>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
struct DestroyProbe {
    wl_listener listener {};
    bool destroyed = false;

    explicit DestroyProbe(wl_signal *signal)
    {
        listener.notify = notify;
        wl_signal_add(signal, &listener);
    }

    ~DestroyProbe()
    {
        if (!destroyed)
            wl_list_remove(&listener.link);
    }

    static void notify(wl_listener *listener, void *)
    {
        DestroyProbe *probe = wl_container_of(listener, probe, listener);
        probe->destroyed = true;
        wl_list_remove(&listener->link);
    }
};

std::shared_ptr<Session> owner;
QString username;
std::list<DestroyProbe> probes;
QList<wl_global *> captureGlobals;
QList<WServerInterface *> interfaces;
WServerInterface *screencopy = nullptr;
QPointer<WSocket> childSocket;

void observe(WServerInterface *interface)
{
    const auto name = interface->interfaceName();
    if (name == "wl_data_device_manager")
        probes.emplace_back(&interface->nativeInterface<wlr_data_device_manager>()->events.destroy);
    else if (name == "zwp_primary_selection_device_manager_v1")
        probes.emplace_back(&interface->nativeInterface<wlr_primary_selection_v1_device_manager>()->events.destroy);
    else if (name == "zwlr_data_control_manager_v1")
        probes.emplace_back(&interface->nativeInterface<wlr_data_control_manager_v1>()->events.destroy);
    else if (name == "ext_data_control_manager_v1")
        probes.emplace_back(&interface->nativeInterface<wlr_ext_data_control_manager_v1>()->events.destroy);
    else if (name == "zwlr_screencopy_manager_v1") {
        probes.emplace_back(&interface->nativeInterface<wlr_screencopy_manager_v1>()->events.destroy);
        screencopy = interface;
    } else if (name == "ext_image_copy_capture_manager_v1")
        captureGlobals.append(interface->nativeInterface<wlr_ext_image_copy_capture_manager_v1>()->global);
    else if (name == "ext_output_image_capture_source_manager_v1")
        captureGlobals.append(interface->nativeInterface<wlr_ext_output_image_capture_source_manager_v1>()->global);
    else if (name == "ext_foreign_toplevel_image_capture_source_manager_v1")
        probes.emplace_back(&interface->nativeInterface<wlr_ext_foreign_toplevel_image_capture_source_manager_v1>()->events.destroy);
    else if (name == "zxdg_exporter_v2") {
        auto *foreign = interface->nativeInterface<wlr_xdg_foreign_v2>();
        probes.emplace_back(&foreign->events.destroy);
        probes.emplace_back(&foreign->registry->events.destroy);
    }
}

int destroyedCount()
{
    int count = 0;
    for (const auto &probe : probes)
        count += probe.destroyed;
    return count;
}
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);
    const auto global = server->sessionManager()->globalSession();
    auto *account = getpwuid(getuid());
    if (account && global && global->username() == QString::fromLocal8Bit(account->pw_name))
        account = getpwnam("nobody");
    if (account)
        username = QString::fromLocal8Bit(account->pw_name);
}

extern "C" void lifecycle_prepare(void *data)
{
    auto *state = static_cast<lifecycle_state *>(data);
    *state = {};
    if (username.isEmpty())
        return;
    probes.clear();
    captureGlobals.clear();
    interfaces.clear();
    screencopy = nullptr;
    auto *runtime = WaylandServer::instance();
    owner = runtime->sessionManager()->ensureSession(QStringLiteral("protocol-lifecycle"), username);
    if (!owner) {
        state->ready = -1;
        return;
    }
    owner->socket()->setEnabled(true);
    const auto name = owner->socket()->fullServerName().toUtf8();
    std::strncpy(state->socket_name, name.constData(), sizeof(state->socket_name) - 1);
    for (auto *interface : runtime->server()->interfaceList()) {
        if (interface->ownsSocket() == owner->socket()) {
            interfaces.append(interface);
            observe(interface);
        }
    }
    childSocket = new WSocket(owner->socket());
    if (!childSocket->autoCreate()) {
        state->ready = -1;
        return;
    }
    runtime->addSocket(childSocket);
    const auto childName = childSocket->fullServerName().toUtf8();
    std::strncpy(state->child_socket_name, childName.constData(), sizeof(state->child_socket_name) - 1);
    state->observed = int(probes.size()) + captureGlobals.size();
    state->ready = 1;
}

extern "C" void lifecycle_detach(void *data)
{
    auto *state = static_cast<lifecycle_state *>(data);
    owner->detachInterface(screencopy);
    owner->detachInterface(screencopy);
    state->detached_once = destroyedCount() == 1
        && !WaylandServer::instance()->server()->interfaceList().contains(screencopy);
}

extern "C" void lifecycle_remove(void *data)
{
    auto *state = static_cast<lifecycle_state *>(data);
    auto *runtime = WaylandServer::instance();
    QList<QPointer<SurfaceWrapper>> windows;
    for (auto *surface : owner->workspace()->surfaces())
        windows.append(surface);
    state->mapped_windows = windows.size();
    // Intentionally retain owner: logout must complete independently of its
    // shared_ptr lifetime, including destruction of mapped child-socket windows.
    runtime->sessionManager()->removeSession(owner);
    state->shutdown_with_reference = owner && owner->isShuttingDown() && !owner->socket();
    state->windows_released = 1;
    for (const auto &window : windows) {
        if (window)
            state->windows_released = 0;
    }
    state->destroyed = destroyedCount();
    owner->shutdown();
    runtime->sessionManager()->removeSession(owner);
    state->shutdown_idempotent = destroyedCount() == state->destroyed;
    // Explicit native destroy retires these globals and clears their user data.
    // Read them synchronously before the event loop can process deferred deletion.
    state->globals_retired = 0;
    for (auto *global : captureGlobals) {
        if (!wl_global_get_user_data(global))
            ++state->globals_retired;
    }
    state->interfaces_removed = 1;
    for (auto *interface : interfaces) {
        if (runtime->server()->interfaceList().contains(interface))
            state->interfaces_removed = 0;
    }
    owner.reset();
    delete childSocket.data();
}
