// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddeshellwayland.h"

#include <private/qwaylandwindow_p.h>

#include <QHash>
#include <QPlatformSurfaceEvent>
#include <QWaylandClientExtension>

#include <functional>
#include <utility>

#define TREELANDDDESHELLMANAGERV2VERSION 2

class DDEShellManageV2
    : public QWaylandClientExtensionTemplate<DDEShellManageV2>
    , public QtWayland::treeland_dde_shell_manager_v2
{
public:
    DDEShellManageV2()
        : QWaylandClientExtensionTemplate<DDEShellManageV2>(TREELANDDDESHELLMANAGERV2VERSION)
    {
        initialize();
    }
};

class DDEShellSurface : public QtWayland::treeland_dde_shell_surface_v2
{
public:
    DDEShellSurface(struct ::treeland_dde_shell_surface_v2 *id,
                    std::function<void(DDEShellWayland::Role)> onRole)
        : QtWayland::treeland_dde_shell_surface_v2(id)
        , m_onRole(std::move(onRole))
    {
    }

    ~DDEShellSurface()
    {
        destroy();
    }

protected:
    void treeland_dde_shell_surface_v2_role(uint32_t role) override
    {
        m_onRole(static_cast<DDEShellWayland::Role>(role));
    }

private:
    std::function<void(DDEShellWayland::Role)> m_onRole;
};

class ShellIntegrationSingleton
{
public:
    ShellIntegrationSingleton();
    std::unique_ptr<DDEShellManageV2> shellManager;
    QHash<QWindow *, DDEShellWayland *> windows;
};

ShellIntegrationSingleton::ShellIntegrationSingleton()
{
    shellManager = std::make_unique<DDEShellManageV2>();
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

    // The QWaylandClientExtension binds the global asynchronously: at first
    // construction it is usually not active yet. Retry creating the shell
    // surface once the manager becomes active, otherwise requests sent before
    // activation would only be cached locally and never reach the compositor.
    connect(s_waylandIntegration->shellManager.get(),
            &DDEShellManageV2::activeChanged,
            this,
            &DDEShellWayland::surfaceCreated);

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
        } else if (surfaceEvent->surfaceEventType()
                   == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            // The v2 protocol requires the shell surface to be destroyed
            // before the wl_surface. Destroy it now; otherwise the shell
            // surface proxy would outlive the wl_surface and the compositor
            // rejects the stale object ("invalid object") on teardown.
            surfaceDestroyed();
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
    m_cursorPlacement.reset();
    if (m_shellSurface) {
        // v2 position hint is output-relative; passing a null output anchors
        // the coordinates at the primary output origin.
        m_shellSurface->set_position_hint(nullptr, position.x(), position.y());
    }
}

void DDEShellWayland::setRole(Role role)
{
    // Applying the same request again must work after a compositor-side change.
    m_role = role;
    if (m_shellSurface && (role != Role::role_always_on_top || protocolVersion() >= 2)) {
        m_shellSurface->set_role(role);
    }
}

void DDEShellWayland::setEffectiveRole(Role role)
{
    m_effectiveRole = role;
    Q_EMIT roleStateChanged();
}

uint32_t DDEShellWayland::protocolVersion() const
{
    return m_shellSurface ? m_shellSurface->version() : 0;
}

void DDEShellWayland::setCursorPlacement(int32_t xOffset, int32_t yOffset)
{
    const QPoint offset(xOffset, yOffset);
    if (offset == m_cursorPlacement) {
        return;
    }

    m_cursorPlacement = offset;
    m_position.reset();
    if (m_shellSurface) {
        m_shellSurface->set_cursor_placement_hint(xOffset, yOffset);
    }
}

void DDEShellWayland::setSkipSwitcher(uint32_t skip)
{
    m_skipFlags = skip ? (m_skipFlags | QtWayland::treeland_dde_shell_surface_v2::skip_flag_switcher)
                       : (m_skipFlags & ~QtWayland::treeland_dde_shell_surface_v2::skip_flag_switcher);
    if (m_shellSurface) {
        m_shellSurface->set_skip_flags(m_skipFlags);
    }
}

void DDEShellWayland::setSkipDockPreview(uint32_t skip)
{
    m_skipFlags = skip ? (m_skipFlags | QtWayland::treeland_dde_shell_surface_v2::skip_flag_dock_preview)
                       : (m_skipFlags & ~QtWayland::treeland_dde_shell_surface_v2::skip_flag_dock_preview);
    if (m_shellSurface) {
        m_shellSurface->set_skip_flags(m_skipFlags);
    }
}

void DDEShellWayland::setSkipMutiTaskView(uint32_t skip)
{
    m_skipFlags = skip ? (m_skipFlags | QtWayland::treeland_dde_shell_surface_v2::skip_flag_multitask_view)
                       : (m_skipFlags & ~QtWayland::treeland_dde_shell_surface_v2::skip_flag_multitask_view);
    if (m_shellSurface) {
        m_shellSurface->set_skip_flags(m_skipFlags);
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
    if (!s_waylandIntegration->shellManager || !s_waylandIntegration->shellManager->isActive()) {
        return;
    }

    // Already created for the current wayland surface: avoid binding the same
    // wl_surface twice (the compositor raises already_shell_surface for a
    // second attempt). activeChanged and surfaceCreated may both fire.
    if (m_shellSurface) {
        return;
    }

    struct wl_surface *surface = nullptr;
    if (auto waylandWindow =
            m_window->nativeInterface<QNativeInterface::Private::QWaylandWindow>()) {
        surface = waylandWindow->surface();
    }

    if (!surface) {
        return;
    }

    auto *id = s_waylandIntegration->shellManager->get_shell_surface(surface);
    if (!id) {
        return;
    }

    m_shellSurface = std::make_unique<DDEShellSurface>(id, [this](Role role) {
        setEffectiveRole(role);
    });
    m_effectiveRole.reset();
    if (m_role) {
        setRole(*m_role);
    }

    if (m_position) {
        m_shellSurface->set_position_hint(nullptr, m_position->x(), m_position->y());
    }

    if (m_cursorPlacement) {
        m_shellSurface->set_cursor_placement_hint(m_cursorPlacement->x(), m_cursorPlacement->y());
    }

    m_shellSurface->set_skip_flags(m_skipFlags);

    if (!m_acceptKeyboardFocus) {
        m_shellSurface->set_accept_keyboard_focus(m_acceptKeyboardFocus);
    }
    Q_EMIT roleStateChanged();
}

void DDEShellWayland::surfaceDestroyed()
{
    if (!m_shellSurface) {
        return;
    }
    m_shellSurface.reset();
    m_effectiveRole.reset();
    Q_EMIT roleStateChanged();
}
