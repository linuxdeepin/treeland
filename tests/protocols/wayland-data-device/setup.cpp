// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wayland-data-device.h"
#include "core/shellhandler.h"
#include "core/waylandserver.h"
#include "server-bridge.h"
#include "surface/surfacewrapper.h"

#include <wbackend.h>
#include <winputdevice.h>
#include <wseat.h>
#include <wlr_all.h>

#include <QPointer>
#include <array>
#include <cstdlib>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
std::array<QPointer<SurfaceWrapper>, 3> toplevels;
unsigned toplevelCount = 0;

void rememberToplevel(SurfaceWrapper *wrapper)
{
    if (wrapper->type() == SurfaceWrapper::Type::XdgToplevel
        && toplevelCount < toplevels.size())
        toplevels[toplevelCount++] = wrapper;
}
}

void protocol_test_setup(WaylandServer *server)
{
    add_headless_output(server->backend(), false);
    auto *keyboard = static_cast<wlr_keyboard *>(std::calloc(1, sizeof(wlr_keyboard)));
    Q_CHECK_PTR(keyboard);
    wlr_keyboard_init(keyboard, nullptr, "clipboard-test-keyboard");
    server->seat()->attachInputDevice(new WInputDevice(&keyboard->base, true));
    QObject::connect(server->shellHandler(), &ShellHandler::surfaceWrapperAdded,
                     server, rememberToplevel);
}

void data_device_focus(void *data)
{
    auto *request = static_cast<data_device_focus_request *>(data);
    request->focused = 0;
    if (request->index >= toplevelCount || !toplevels[request->index])
        return;

    auto *server = WaylandServer::instance();
    if (request->clear_first)
        server->requestKeyboardFocus(nullptr, Qt::OtherFocusReason);
    auto *target = toplevels[request->index].data();
    server->requestKeyboardFocus(target, Qt::ActiveWindowFocusReason);
    request->focused = server->seat()->keyboardFocusSurface() == target->surface();
}
