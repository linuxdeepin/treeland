// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "session-isolation.h"
#include "session/sessionmanager.h"
#include "server-bridge.h"
#include "core/waylandserver.h"
#include "core/shellhandler.h"
#include "modules/foreign-toplevel/foreigntoplevelmanagerv3.h"
#include "session/session.h"
#include "desktop/desktopenvironment.h"
#include "session/sessionprotocols.h"
#include "session/sessionprelaunch.h"
#include "session/sessionscene.h"
#include "modules/prelaunch-splash/prelaunchsplash.h"
#include "dconfig/dconfigmanager.h"
#include "appconfig.hpp"
#include "session/sessionseatmanager.h"
#include "session/sessioninputchannel.h"
#include "input/compositorinputfilter.h"
#include "modules/shortcut/shortcutmanager.h"
#include "seat/seatmanager.h"
#include <wseat.h>
#include <QKeyEvent>
#include "session/sessionoutputmanager.h"
#include "session/sessionswitcher.h"
#include "input/inputmanager.h"
#include "wallpaper/wallpapermanager.h"
#include "workspace/workspace.h"
#include "output/output.h"
#include "outputconfig.hpp"
#include <woutput.h>
#include "core/rootsurfacecontainer.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>
#include <wserver.h>
#include <wsocket.h>
#include <wayland-server-core.h>
#include <wlr_all.h>
#include <pwd.h>
#include <unistd.h>
#include <cstring>

namespace {
std::shared_ptr<Session> sessionA;
std::shared_ptr<Session> sessionB;
int previewSurfaces = -1;
int previewRequests = 0;
int foreignPreviewSurfaces = 0;
QPointer<WSocket> backgroundSocket;

// Observe production interface objects in addition to the C registry events.
bool hasIndependentForeignInterfaces(Session *owner)
{
    WServerInterface *exporter = nullptr;
    WServerInterface *importer = nullptr;
    for (auto *interface : WaylandServer::instance()->server()->interfaceList()) {
        if (interface->ownsSocket() != owner->socket())
            continue;
        if (interface->interfaceName() == "zxdg_exporter_v2")
            exporter = interface;
        else if (interface->interfaceName() == "zxdg_importer_v2")
            importer = interface;
    }
    return exporter && importer && exporter != importer && exporter->handle()
        && exporter->handle() == importer->handle();
}
}

void protocol_test_setup(WaylandServer *server)
{
    for (int i = server->backend()->outputList().size(); i < 3; ++i)
        add_headless_output(server->backend(), false);
    DConfigManager::instance()->appConfig(QStringLiteral("org.deepin.session-launch-test"));
    sessionA = server->sessionManager()->globalSession();
    const auto *account = getpwuid(getuid());
    if (account && sessionA && sessionA->username() == QString::fromLocal8Bit(account->pw_name))
        account = getpwnam("nobody");
    if (!account || !sessionA)
        return;
    sessionB = server->sessionManager()->ensureSession(
        QStringLiteral("protocol-session-isolation"), QString::fromLocal8Bit(account->pw_name));
    if (!sessionB || sessionA == sessionB)
        return;
    // Keep the background test connection dispatchable. Visibility/authorization
    // still use the real Session identity, independently of socket suspension.
    sessionB->socket()->setEnabled(true);
    QObject::connect(sessionB->windowManagement(),
                     &ForeignToplevelManagerInterfaceV3::requestDockPreview,
                     server, [](auto surfaces, WSurface *, QPoint, auto) {
                         previewSurfaces = static_cast<int>(surfaces.size());
                         ++previewRequests;
                         for (auto *surface : surfaces) {
                             if (surface->session() != sessionB.get())
                                 ++foreignPreviewSurfaces;
                         }
                     });
}

extern "C" bool protocol_test_ready(WaylandServer *server)
{
    if (!sessionA || !sessionB)
        return true; // The C client reports the missing-account skip.
    if (server->rootSurfaceContainer()->outputs().size() != 3)
        return false;
    auto *launchConfig = DConfigManager::instance()->appConfig(QStringLiteral("org.deepin.session-launch-test"));
    if (!configurationReady(launchConfig))
        return false;
    auto *a = sessionA->outputManager()->protocol()->handle();
    auto *b = sessionB->outputManager()->protocol()->handle();
    return a && b && a != b && wl_list_length(&a->heads) == 3 && wl_list_length(&b->heads) == 3;
}

extern "C" void isolation_read(void *data)
{
    auto *state = static_cast<isolation_state *>(data);
    *state = {};
    if (!sessionA || !sessionB || sessionA == sessionB)
        return;
    const auto socket = sessionB->socket()->fullServerName().toUtf8();
    std::strncpy(state->socket_name, socket.constData(), sizeof(state->socket_name) - 1);
    state->ready = 1;
    if (auto *output = WaylandServer::instance()->rootSurfaceContainer()->primaryOutput()) {
        auto *a = sessionA->outputManager()->outputState(output);
        auto *b = sessionB->outputManager()->outputState(output);
        state->independent_outputs = a != b && a->windows() != b->windows()
            && a->minimizedSurfaces() != b->minimizedSurfaces()
            && sessionA->outputManager()->config(output->getOutputId())
                == sessionB->outputManager()->config(output->getOutputId());
        state->a_scale = sessionA->outputManager()->config(output->getOutputId())->scale();
        state->b_scale = sessionB->outputManager()->config(output->getOutputId())->scale();
        state->hardware_scale = output->output()->scale();
        state->a_logical_width = a->geometry().width();
        state->b_logical_width = b->geometry().width();
        state->a_bottom_zone = a->exclusiveZone().bottom();
        state->b_bottom_zone = b->exclusiveZone().bottom();
        state->active_bottom_zone = output->exclusiveZone().bottom();
        const auto aLayers = a->layerSurfaces();
        const auto bLayers = b->layerSurfaces();
        state->a_panel_at_bottom = aLayers.size() == 1
            && qAbs(aLayers.first()->y() + aLayers.first()->height() - a->geometry().bottom()) < 1;
        state->b_panel_at_bottom = bLayers.size() == 1
            && qAbs(bLayers.first()->y() + bLayers.first()->height() - b->geometry().bottom()) < 1;
    }
    state->inactive_surfaces_stopped = 1;
    for (auto *owner : {sessionA.get(), sessionB.get()}) {
        if (owner->isActive())
            continue;
        for (auto *surface : owner->desktop()->surfaces()->objects()) {
            // These fixtures contain ordinary user windows and panels only.
            state->inactive_surfaces_stopped &= !surface->renderingEnabled()
                && !surface->isVisible() && !surface->isEnabled()
                && (!surface->surfaceItem()
                    || surface->surfaceItem()->flags().testFlag(WSurfaceItem::NonLive));
        }
    }
    state->a_visible_windows = 0;
    state->b_visible_windows = 0;
    for (auto *window : sessionA->windows()->objects())
        state->a_visible_windows += window->isVisible();
    for (auto *window : sessionB->windows()->objects())
        state->b_visible_windows += window->isVisible();
    state->a_windows = sessionA->windows()->rowCount();
    state->b_windows = sessionB->windows()->rowCount();
    state->independent_workspaces = sessionA->workspace() != sessionB->workspace();
    state->a_current_workspace = sessionA->workspace()->currentIndex();
    state->b_current_workspace = sessionB->workspace()->currentIndex();
    state->preview_surfaces = previewSurfaces;
    state->preview_requests = previewRequests;
    state->foreign_preview_surfaces = foreignPreviewSurfaces;
    const auto *aProtocols = sessionA->protocols();
    const auto *bProtocols = sessionB->protocols();
    state->independent_protocol_domains = sessionA->prelaunch() != sessionB->prelaunch()
        && sessionA->prelaunch()->protocol() != sessionB->prelaunch()->protocol()
        && aProtocols != bProtocols
        && aProtocols->xdgShell() != bProtocols->xdgShell()
        && aProtocols->layerShell() != bProtocols->layerShell()
        && aProtocols->xdgDecoration() != bProtocols->xdgDecoration()
        && aProtocols->xdgDialog() != bProtocols->xdgDialog()
        && aProtocols->xdgTags() != bProtocols->xdgTags()
        && aProtocols->personalization() != bProtocols->personalization()
        && aProtocols->decoration() != bProtocols->decoration()
        && hasIndependentForeignInterfaces(sessionA.get())
        && hasIndependentForeignInterfaces(sessionB.get());
    auto *aSeat = sessionA->seatManager()->fallbackSeat();
    auto *bSeat = sessionB->seatManager()->fallbackSeat();
    state->independent_seats = aSeat && bSeat && aSeat != bSeat
        && aSeat->handle() != bSeat->handle()
        && sessionA->seatManager()->inputMethodHelper(aSeat)
            != sessionB->seatManager()->inputMethodHelper(bSeat);
    state->protocol_socket_ownership = aSeat && bSeat
        && aSeat->ownsSocket() == sessionA->socket()
        && bSeat->ownsSocket() == sessionB->socket()
        && !aSeat->ownsClient(nullptr) && !bSeat->ownsClient(nullptr);
    if (aSeat && bSeat) {
        for (auto *client : sessionA->socket()->clients())
            state->protocol_socket_ownership &= aSeat->ownsClient(client) && !bSeat->ownsClient(client);
        for (auto *client : sessionB->socket()->clients())
            state->protocol_socket_ownership &= bSeat->ownsClient(client) && !aSeat->ownsClient(client);
        if (backgroundSocket) {
            for (auto *client : backgroundSocket->clients())
                state->protocol_socket_ownership &= bSeat->ownsClient(client) && !aSeat->ownsClient(client);
        }
    }
    const auto active = WaylandServer::instance()->sessionManager()->activeSessionObject();
    state->independent_seats = state->independent_seats && active
        && WaylandServer::instance()->seat() == active->seatManager()->fallbackSeat();
    auto *aDesktop = sessionA->desktop();
    auto *bDesktop = sessionB->desktop();
    state->independent_desktops = sessionA->scene() != sessionB->scene()
        && aDesktop && bDesktop && aDesktop != bDesktop
        && aDesktop->viewRoot() != bDesktop->viewRoot()
        && aDesktop->input()->parent() == aDesktop
        && bDesktop->input()->parent() == bDesktop
        && aDesktop->outputs()->parent() == aDesktop
        && bDesktop->outputs()->parent() == bDesktop
        && aDesktop->surfaces() != bDesktop->surfaces();
    for (auto *desktop : {aDesktop, bDesktop}) {
        const auto views = desktop->viewRoot()->childItems();
        state->independent_desktops = state->independent_desktops && !views.isEmpty();
        for (auto *view : views)
            state->independent_desktops = state->independent_desktops
                && view->property("desktop").value<DesktopEnvironment *>() == desktop;
    }
    auto *aInput = aDesktop->inputChannel();
    auto *bInput = bDesktop->inputChannel();
    state->independent_desktops = state->independent_desktops
        && aInput != bInput && aInput->filter() != bInput->filter()
        && aInput->gestures() != bInput->gestures()
        && aInput->shortcuts() != bInput->shortcuts()
        && aInput->shortcutProtocol() && bInput->shortcutProtocol()
        && aInput->shortcutProtocol() != bInput->shortcutProtocol()
        && aInput->shortcutProtocol()->controller() == aInput->shortcuts()
        && bInput->shortcutProtocol()->controller() == bInput->shortcuts()
        && aSeat->eventFilter() == aInput->filter()
        && bSeat->eventFilter() == bInput->filter();
    const QKeyEvent f9(QEvent::KeyPress, Qt::Key_F9, Qt::NoModifier);
    const QKeyEvent f10(QEvent::KeyPress, Qt::Key_F10, Qt::NoModifier);
    state->a_shortcut = aInput->shortcuts()->matchesShortcut(&f9);
    state->b_shortcut = bInput->shortcuts()->matchesShortcut(&f10);
    state->foreign_shortcut = aInput->shortcuts()->matchesShortcut(&f10)
        || bInput->shortcuts()->matchesShortcut(&f9);
    state->independent_managers = sessionA->windowManagement() != sessionB->windowManagement()
        && sessionA->inputManager() != sessionB->inputManager()
        && sessionA->wallpaperManager() != sessionB->wallpaperManager()
        && sessionA->outputManager() != sessionB->outputManager()
        && sessionA->switcher() != sessionB->switcher();
    state->active_managers = sessionA->inputManager()->hardwareAttached() == sessionA->isActive()
        && sessionB->inputManager()->hardwareAttached() == sessionB->isActive()
        && sessionA->outputManager()->hardwareAttached() == sessionA->isActive()
        && sessionB->outputManager()->hardwareAttached() == sessionB->isActive();
}

// Directed UI stimulus: exercise the production SessionOutputManager scale
// entry point used by OutputMenuBar, not a fabricated Wayland protocol result.
extern "C" void isolation_scale_a(void *)
{
    auto *output = WaylandServer::instance()->rootSurfaceContainer()->primaryOutput();
    if (output)
        sessionA->outputManager()->setScale(output->output(), 1.5);
}

extern "C" void isolation_switch_b(void *)
{
    sessionA->workspace()->createModel(QStringLiteral("session-A-second"));
    sessionA->workspace()->switchTo(1, false);
    WaylandServer::instance()->sessionManager()->updateActiveUserSession(sessionB->username(), sessionB->id());
    previewSurfaces = -1;
    previewRequests = 0;
    foreignPreviewSurfaces = 0;
}

extern "C" void isolation_reactivate_b(void *)
{
    WaylandServer::instance()->sessionManager()->updateActiveUserSession(sessionB->username(), sessionB->id());
}

extern "C" void isolation_recreate_b(void *data)
{
    auto *state = static_cast<isolation_state *>(data);
    *state = {};
    if (!sessionB)
        return;
    auto *manager = WaylandServer::instance()->sessionManager();
    const auto id = sessionB->id();
    const auto username = sessionB->username();
    const std::weak_ptr<Session> previous = sessionB;
    manager->removeSession(sessionB);
    sessionB.reset();
    if (!previous.expired())
        return;
    sessionB = manager->ensureSession(id, username);
    if (!sessionB)
        return;
    sessionB->socket()->setEnabled(true);
    const auto name = sessionB->socket()->fullServerName().toUtf8();
    std::strncpy(state->socket_name, name.constData(), sizeof(state->socket_name) - 1);
    state->ready = 1;
}

extern "C" void isolation_restore_a(void *)
{
    const auto clients = sessionB->socket()->clients();
    for (auto *client : clients)
        wl_client_destroy(client->handle());
    WaylandServer::instance()->sessionManager()->updateActiveUserSession(sessionA->username(), sessionA->id());
    // Avoid process suspension when exercising both clients in one test process.
    sessionB->socket()->setEnabled(true);
}

// A real child connection models the socket hierarchy used by XWayland.
// Disable B only after its direct test clients were disconnected, so the
// in-process harness is never SIGSTOP'ed by the production socket freezer.
extern "C" void isolation_prepare_background(void *data)
{
    auto *state = static_cast<isolation_state *>(data);
    *state = {};
    if (!sessionB->socket()->clients().isEmpty())
        return;
    sessionA->workspace()->switchTo(0, false);
    sessionB->socket()->setEnabled(false);
    if (!backgroundSocket) {
        auto *server = WaylandServer::instance();
        auto *socket = new WSocket(sessionB->socket(), server);
        if (!socket->autoCreate()) {
            delete socket;
            return;
        }
        server->addSocket(socket);
        backgroundSocket = socket;
    }
    const auto name = backgroundSocket->fullServerName().toUtf8();
    std::strncpy(state->socket_name, name.constData(), sizeof(state->socket_name) - 1);
    state->ready = 1;
}
