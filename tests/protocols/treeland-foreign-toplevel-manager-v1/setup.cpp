// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "modules/foreign-toplevel/foreigntoplevelmanagerv1.h"
#include "protocol-test-server.h"
#include "treeland-foreign-toplevel-manager-v1.h"

#include <qwxdgshell.h>
#include <wserver.h>
#include <wsurface.h>
#include <wxdgshell.h>
#include <wxdgtoplevelsurface.h>

#include <cstring>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
ForeignToplevelManagerInterfaceV1 *g_manager = nullptr;
/* The WSurface the client passed to get_dock_preview_context, captured from the
 * requestDockPreview signal; used to drive enterDockPreview/leaveDockPreview. */
WSurface *g_relativeSurface = nullptr;
struct ftm_server_state g_state {};
}

void protocol_test_setup(WServer *server)
{
    /* The client creates an xdg_toplevel so its surface carries a waylib
     * WSurface wrapper; without it the dock preview context cannot resolve the
     * relative surface and every show/enter/leave is a silent no-op. */
    protocol_test_create_headless_output(server);
    protocol_test_enable_shm(server);

    auto *xdgShell = server->attach<WXdgShell>(5);
    QObject::connect(xdgShell, &WXdgShell::toplevelSurfaceAdded, xdgShell,
                     [](WXdgToplevelSurface *toplevel) {
                         auto *surface = toplevel->surface();
                         QObject::connect(surface, &WSurface::mappedChanged, surface, [surface] {
                             if (surface->mapped())
                                 g_state.mapped_xdg_toplevel = 1;
                         });
                         auto *connection = new QMetaObject::Connection;
                         *connection = QObject::connect(surface, &WSurface::commit, surface,
                                                        [toplevel, connection] {
                             qw_xdg_surface::from(toplevel->handle()->handle()->base)->schedule_configure();
                             QObject::disconnect(*connection);
                             delete connection;
                         });
                     });

    g_manager = server->attach<ForeignToplevelManagerInterfaceV1>();
    QObject::connect(g_manager, &ForeignToplevelManagerInterfaceV1::requestDockPreview,
                     [](auto surfaces, WSurface *target, QPoint abs, auto direction) {
                         g_state.preview_fired = 1;
                         g_state.preview_x = abs.x();
                         g_state.preview_y = abs.y();
                         g_state.preview_direction = static_cast<uint32_t>(direction);
                         g_state.preview_surface_count = static_cast<int>(surfaces.size());
                         g_relativeSurface = target;
                     });
    QObject::connect(g_manager, &ForeignToplevelManagerInterfaceV1::requestDockPreviewTooltip,
                     [](QString tooltip, WSurface *, QPoint abs, auto direction) {
                         g_state.tooltip_fired = 1;
                         const QByteArray utf8 = tooltip.toUtf8();
                         std::strncpy(g_state.tooltip, utf8.constData(),
                                      sizeof(g_state.tooltip) - 1);
                         g_state.tooltip[sizeof(g_state.tooltip) - 1] = '\0';
                         g_state.tooltip_x = abs.x();
                         g_state.tooltip_y = abs.y();
                         g_state.tooltip_direction = static_cast<uint32_t>(direction);
                     });
    QObject::connect(g_manager, &ForeignToplevelManagerInterfaceV1::requestDockClose,
                     []() { g_state.close_fired = 1; });
}

extern "C" void ftm_read_server_state(void *data)
{
    auto *state = static_cast<struct ftm_server_state *>(data);
    *state = g_state;
}

extern "C" void ftm_enter_dock_preview(void *)
{
    if (g_manager && g_relativeSurface)
        g_manager->enterDockPreview(g_relativeSurface);
}

extern "C" void ftm_leave_dock_preview(void *)
{
    if (g_manager && g_relativeSurface)
        g_manager->leaveDockPreview(g_relativeSurface);
}
