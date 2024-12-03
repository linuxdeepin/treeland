// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddeshellwayland.h"

#include <private/qwaylandwindow_p.h>

#include <QHash>
#include <QPlatformSurfaceEvent>
#include <QWaylandClientExtension>

#define TREELANDDDESHELLMANAGERV1VERSION 1

class DDEShellManageV1
    : public QWaylandClientExtensionTemplate<DDEShellManageV1>
    , public QtWayland::treeland_dde_shell_manager_v1
{
public:
    DDEShellManageV1()
        : QWaylandClientExtensionTemplate<DDEShellManageV1>(TREELANDDDESHELLMANAGERV1VERSION)
    {
        initialize();
    }
};

class DDEShellSurface : public QtWayland::treeland_dde_shell_surface_v1
{
public:
    DDEShellSurface(struct ::treeland_dde_shell_surface_v1 *id)
        : QtWayland::treeland_dde_shell_surface_v1(id)
    {
    }

    ~DDEShellSurface()
    {
        destroy();
    }
};

class ShellIntegrationSingleton
{
public:
    ShellIntegrationSingleton();
    std::unique_ptr<DDEShellManageV1> shellManager;
    QHash<QWindow *, DDEShellWayland *> windows;
};

ShellIntegrationSingleton::ShellIntegrationSingleton()
{
    shellManager = std::make_unique<DDEShellManageV1>();
}

Q_GLOBAL_STATIC(ShellIntegrationSingleton, s_waylandIntegration)

DDEShellWayland *DDEShellWayland::get(QWindow *window)
{
    DDEShellWayland *&it = s_waylandIntegration->windows[window];
    if (!it) {
        it = new DDEShellWayland(window);
    }
    return it;
}

DDEShellWayland::~DDEShellWayland()
{
    s_waylandIntegration->windows.remove(m_window);
}

DDEShellWayland::DDEShellWayland(QWindow *window)
    : QObject(window)
    , m_window(window)
{
    m_window->installEventFilter(this);
    platformSurfaceCreated(window);
}

bool DDEShellWayland::eventFilter(QObject *watched, QEvent *event)
{
    auto window = qobject_cast<QWindow *>(watched);
    if (!window) {
        return false;
    }
    if (event->type() == QEvent::PlatformSurface) {
        auto surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
        if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
            platformSurfaceCreated(window);
        }
    }
    return false;
}

void DDEShellWayland::setPosition(const QPoint &position)
{
    if (position == m_position) {
        return;
    }

    m_position = position;
    if (m_shellSurface) {
        m_shellSurface->set_surface_position(m_position->x(), m_position->y());
    }
}

void DDEShellWayland::setRole(QtWayland::treeland_dde_shell_surface_v1::role role)
{
    if (role == m_role) {
        return;
    }

    m_role = role;
    if (m_shellSurface) {
        m_shellSurface->set_role(role);
    }
}

void DDEShellWayland::setAutoPlacement(int32_t yOffset)
{
    if (yOffset == m_yOffset) {
        return;
    }

    m_yOffset = yOffset;
    if (m_shellSurface) {
        m_shellSurface->set_auto_placement(yOffset);
    }
}

void DDEShellWayland::setSkipSwitcher(uint32_t skip)
{
    if (skip == m_skipSwitcher) {
        return;
    }

    m_skipSwitcher = skip;
    if (m_shellSurface) {
        m_shellSurface->set_skip_switcher(skip);
    }
}

void DDEShellWayland::setSkipDockPreview(uint32_t skip)
{
    if (skip == m_skipDockPreview) {
        return;
    }

    m_skipDockPreview = skip;
    if (m_shellSurface) {
        m_shellSurface->set_skip_dock_preview(skip);
    }
}

void DDEShellWayland::setSkipMutiTaskView(uint32_t skip)
{
    if (skip == m_skipMutiTaskView) {
        return;
    }

    m_skipMutiTaskView = skip;
    if (m_shellSurface) {
        m_shellSurface->set_skip_muti_task_view(skip);
    }
}

void DDEShellWayland::setAcceptKeyboardFocus(uint32_t accept)
{
    if (accept == m_acceptKeyboardFocus) {
        return;
    }

    m_acceptKeyboardFocus = accept;
    if (m_shellSurface) {
        m_shellSurface->set_accept_keyboard_focus(accept);
    }
}

void DDEShellWayland::platformSurfaceCreated(QWindow *window)
{
    auto waylandWindow = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    if (!waylandWindow) {
        return;
    }
    connect(waylandWindow,
            &QNativeInterface::Private::QWaylandWindow::surfaceCreated,
            this,
            &DDEShellWayland::surfaceCreated);
    connect(waylandWindow,
            &QNativeInterface::Private::QWaylandWindow::surfaceDestroyed,
            this,
            &DDEShellWayland::surfaceDestroyed);
    if (waylandWindow->surface()) {
        surfaceCreated();
    }
}

void DDEShellWayland::surfaceCreated()
{
    struct wl_surface *surface = nullptr;
    if (!s_waylandIntegration->shellManager || !s_waylandIntegration->shellManager->isActive()) {
        return;
    }

    if (auto waylandWindow =
            m_window->nativeInterface<QNativeInterface::Private::QWaylandWindow>()) {
        surface = waylandWindow->surface();
    }

    if (!surface) {
        return;
    }

    m_shellSurface = std::make_unique<DDEShellSurface>(
        s_waylandIntegration->shellManager->get_shell_surface(surface));
    if (m_shellSurface) {
        m_shellSurface->set_role(m_role);

        if (m_position) {
            m_shellSurface->set_surface_position(m_position->x(), m_position->y());
        }

        if (m_yOffset) {
            m_shellSurface->set_auto_placement(m_yOffset.value());
        }

        if (m_skipDockPreview) {
            m_shellSurface->set_skip_dock_preview(m_skipDockPreview.value());
        }

        if (m_skipMutiTaskView) {
            m_shellSurface->set_skip_muti_task_view(m_skipMutiTaskView.value());
        }

        if (m_skipSwitcher) {
            m_shellSurface->set_skip_switcher(m_skipSwitcher.value());
        }

        if (!m_acceptKeyboardFocus) {
            m_shellSurface->set_accept_keyboard_focus(m_acceptKeyboardFocus);
        }
    }
}

void DDEShellWayland::surfaceDestroyed()
{
    m_shellSurface.reset();
}
