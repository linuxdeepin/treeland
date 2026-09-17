// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "compositoractioninterfacev1.h"

#include "qwayland-server-treeland-compositor-action-unstable-v1.h"

#include <wayland-server-core.h>

std::optional<ShellAction> CompositorActionInterfaceV1::mapCompositorAction(uint32_t protocolAction)
{
    using Action = QtWaylandServer::treeland_compositor_action_v1::action;

    switch (protocolAction) {
    case Action::action_workspace_1:
        return ShellAction::SwitchWorkspace1;
    case Action::action_workspace_2:
        return ShellAction::SwitchWorkspace2;
    case Action::action_workspace_3:
        return ShellAction::SwitchWorkspace3;
    case Action::action_workspace_4:
        return ShellAction::SwitchWorkspace4;
    case Action::action_workspace_5:
        return ShellAction::SwitchWorkspace5;
    case Action::action_workspace_6:
        return ShellAction::SwitchWorkspace6;
    case Action::action_workspace_7:
        return ShellAction::SwitchWorkspace7;
    case Action::action_workspace_8:
        return ShellAction::SwitchWorkspace8;
    case Action::action_workspace_9:
        return ShellAction::SwitchWorkspace9;
    case Action::action_workspace_10:
        return ShellAction::SwitchWorkspace10;
    case Action::action_workspace_11:
        return ShellAction::SwitchWorkspace11;
    case Action::action_workspace_12:
        return ShellAction::SwitchWorkspace12;
    case Action::action_prev_workspace:
        return ShellAction::PreviousWorkspace;
    case Action::action_next_workspace:
        return ShellAction::NextWorkspace;
    case Action::action_show_desktop:
        return ShellAction::ToggleShowDesktop;
    case Action::action_open_multitask_view:
        return ShellAction::OpenMultitaskView;
    case Action::action_close_multitask_view:
        return ShellAction::CloseMultitaskView;
    case Action::action_toggle_multitask_view:
        return ShellAction::ToggleMultitaskView;
    case Action::action_toggle_fps_display:
        return ShellAction::ToggleFpsDisplay;
    case Action::action_zoom_in:
        return ShellAction::ZoomIn;
    case Action::action_zoom_out:
        return ShellAction::ZoomOut;
    case Action::action_zoom_reset:
        return ShellAction::ZoomReset;
    case Action::action_lockscreen:
        return ShellAction::LockScreen;
    case Action::action_shutdown_menu:
        return ShellAction::ShowShutdownMenu;
    case Action::action_shutdown_menu_power_off:
        return ShellAction::ShowShutdownMenuPowerOff;
    case Action::action_shutdown_menu_reboot:
        return ShellAction::ShowShutdownMenuReboot;
    case Action::action_shutdown_menu_suspend:
        return ShellAction::ShowShutdownMenuSuspend;
    case Action::action_shutdown_menu_hibernate:
        return ShellAction::ShowShutdownMenuHibernate;
    case Action::action_shutdown_menu_log_out:
        return ShellAction::ShowShutdownMenuLogOut;
    case Action::action_show_user_switch:
        return ShellAction::ShowUserSwitch;
    default:
        // Unknown values are ignored per protocol.
        return std::nullopt;
    }
}

// ---------------------------------------------------------------------------
// CompositorActionInterfaceV1Private
// ---------------------------------------------------------------------------

class CompositorActionInterfaceV1Private
    : public QtWaylandServer::treeland_compositor_action_v1
{
public:
    explicit CompositorActionInterfaceV1Private(CompositorActionInterfaceV1 *_q);
    ~CompositorActionInterfaceV1Private() override = default;

    wl_global *global() const { return m_global; }

    CompositorActionInterfaceV1 *q = nullptr;

protected:
    void destroy(Resource *resource) override;
    void trigger(Resource *resource, uint32_t action) override;
};

CompositorActionInterfaceV1Private::CompositorActionInterfaceV1Private(CompositorActionInterfaceV1 *_q)
    : QtWaylandServer::treeland_compositor_action_v1()
    , q(_q)
{
}

void CompositorActionInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void CompositorActionInterfaceV1Private::trigger([[maybe_unused]] Resource *resource,
                                                 uint32_t action)
{
    // Fire-and-forget: forward the raw value, no reply is sent.
    Q_EMIT q->triggered(action);
}

// ---------------------------------------------------------------------------
// CompositorActionInterfaceV1
// ---------------------------------------------------------------------------

CompositorActionInterfaceV1::CompositorActionInterfaceV1(QObject *parent)
    : QObject(parent)
    , WServerInterface()
    , d(std::make_unique<CompositorActionInterfaceV1Private>(this))
{
}

CompositorActionInterfaceV1::~CompositorActionInterfaceV1() = default;

QByteArrayView CompositorActionInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

void CompositorActionInterfaceV1::create(WServer *server)
{
    d->init(server->handle(), InterfaceVersion);
}

void CompositorActionInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *CompositorActionInterfaceV1::global() const
{
    return d->global();
}
