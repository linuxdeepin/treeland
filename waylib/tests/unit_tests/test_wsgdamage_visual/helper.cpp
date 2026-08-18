// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include <WBackend>
#include <WCursor>
#include <WOutput>
#include <WSeat>
#include <WServer>
#include <WXdgShell>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wquickoutputlayout.h>
#include <wrenderhelper.h>
#include <wsocket.h>
#include <wsurface.h>
#include <wxdgtoplevelsurface.h>

#include <wayland-server-core.h>
#include <wlr/types/wlr_xdg_shell.h>

#include <QCursor>
#include <QDebug>
#include <QQmlEngine>

#include <sys/socket.h>
#include <unistd.h>

namespace {
constexpr int kSceneWidth = 800;
constexpr int kSceneHeight = 480;
}

VisualHelper::VisualHelper(QObject *parent)
    : QObject(parent)
    , m_server(new WServer(this))
    , m_outputCreator(new WQmlCreator(this))
    , m_xdgShellCreator(new WQmlCreator(this))
    , m_outputLayout(new WQuickOutputLayout(m_server))
    , m_cursor(new WCursor(this))
{
    m_seat = m_server->attach<WSeat>();
    m_seat->setCursor(m_cursor);
    m_cursor->setLayout(m_outputLayout);
    m_cursor->setCursor(QCursor(Qt::ArrowCursor));
    m_cursor->setVisible(true);
}

void VisualHelper::initProtocols(WOutputRenderWindow *window, QQmlEngine *qmlEngine)
{
    m_backend = m_server->attach<WBackend>();
    m_server->start();

    m_renderer = WRenderHelper::createRenderer(m_backend->handle());
    if (!m_renderer) {
        qWarning("Failed to create wlroots renderer");
        return;
    }

    connect(m_backend, &WBackend::outputAdded, this, [this, qmlEngine](WOutput *output) {
        auto props = qmlEngine->newObject();
        props.setProperty("waylandOutput", qmlEngine->toScriptValue(output));
        props.setProperty("layout", qmlEngine->toScriptValue(m_outputLayout));
        props.setProperty("x", qmlEngine->toScriptValue(m_outputLayout->implicitWidth()));
        m_outputCreator->add(output, props);
    });
    connect(m_backend, &WBackend::outputRemoved, this, [this](WOutput *output) {
        m_outputCreator->removeByOwner(output);
    });
    connect(m_backend, &WBackend::inputAdded, this, [this](WInputDevice *device) {
        m_seat->attachInputDevice(device);
    });
    connect(m_backend, &WBackend::inputRemoved, this, [this](WInputDevice *device) {
        m_seat->detachInputDevice(device);
    });

    m_socket = new WSocket(false, this);
    if (m_socket->autoCreate()) {
        m_server->addSocket(m_socket);
    } else {
        delete m_socket;
        m_socket = nullptr;
        qCritical("Failed to create Wayland socket for the visual damage test");
    }

    m_allocator = wlr_allocator_autocreate(m_backend->handle(), m_renderer);
    wlr_renderer_init_wl_display(m_renderer, m_server->handle());
    m_compositor = wlr_compositor_create(m_server->handle(), 6, m_renderer);
    wlr_subcompositor_create(m_server->handle());

    auto *xdgShell = m_server->attach<WXdgShell>(5);
    connect(xdgShell, &WXdgShell::toplevelSurfaceAdded, this,
            [this, qmlEngine](WXdgToplevelSurface *surface) {
        connect(surface->surface(), &WSurface::commit, surface, [surface] {
            if (surface->handle() && surface->handle()->base
                && surface->handle()->base->initial_commit)
                wlr_xdg_toplevel_set_size(surface->handle(), 0, 0);
        }, Qt::DirectConnection);

        auto props = qmlEngine->newObject();
        props.setProperty("waylandSurface", qmlEngine->toScriptValue(surface));
        m_xdgShellCreator->add(surface, props);
    });
    connect(xdgShell, &WXdgShell::toplevelSurfaceRemoved,
            m_xdgShellCreator, &WQmlCreator::removeByOwner);

    connect(window, &WOutputRenderWindow::outputViewportInitialized, this,
            [](WOutputViewport *viewport) {
        auto *output = viewport->output();
        if (output->property("_Enabled").toBool())
            return;
        output->setProperty("_Enabled", true);
        auto *wlrOutput = output->handle();
        wlr_output_state newState;
        wlr_output_state_init(&newState);
        wlr_output_state_set_custom_mode(&newState, kSceneWidth, kSceneHeight, 0);
        wlr_output_state_set_enabled(&newState, true);
        if (!wlr_output_commit_state(wlrOutput, &newState))
            qCritical("commit failed on output %s", wlrOutput->name);
        wlr_output_state_finish(&newState);
    });

    window->init(m_renderer, m_allocator);
    wlr_backend_start(m_backend->handle());
}

QString VisualHelper::waylandSocketName() const
{
    return m_socket ? m_socket->fullServerName() : QString();
}

int VisualHelper::createInProcessClientFd()
{
    if (!m_socket)
        return -1;

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds) < 0)
        return -1;
    if (!m_socket->addClient(fds[0])) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    return fds[1];
}

void VisualHelper::dispatchWaylandEvents()
{
    if (!m_server || !m_server->handle())
        return;
    struct wl_event_loop *loop = wl_display_get_event_loop(m_server->handle());
    wl_event_loop_dispatch(loop, 0);
    wl_event_loop_dispatch_idle(loop);
    wl_display_flush_clients(m_server->handle());
}
