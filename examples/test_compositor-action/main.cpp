// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Simple test client for the treeland-compositor-action-unstable-v1 protocol.
//
// Usage:
//   test-compositor-action list            print all action names and values
//   test-compositor-action <name|value>    trigger one action, e.g.
//                                          ./test-compositor-action workspace_2
//                                          ./test-compositor-action show_desktop
//                                          ./test-compositor-action 23 (lockscreen)
//
// The protocol is fire-and-forget: the compositor sends no reply, so the
// client just triggers the action and exits.

#include "qwayland-treeland-compositor-action-unstable-v1.h"

#include <QGuiApplication>
#include <QTimer>
#include <QtWaylandClient/QWaylandClientExtension>
#include <QtWaylandClient/private/qwaylandintegration_p.h>

#include <wayland-client-core.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

class CompositorAction
    : public QWaylandClientExtensionTemplate<CompositorAction>
    , public QtWayland::treeland_compositor_action_v1
{
    Q_OBJECT
public:
    explicit CompositorAction()
        : QWaylandClientExtensionTemplate<CompositorAction>(1)
    {
    }
};

struct ActionName {
    const char *name;
    uint32_t value;
};

// Keep in sync with the action enum of treeland-compositor-action-unstable-v1.
static const ActionName kActions[] = {
    { "workspace_1", CompositorAction::action_workspace_1 },
    { "workspace_2", CompositorAction::action_workspace_2 },
    { "workspace_3", CompositorAction::action_workspace_3 },
    { "workspace_4", CompositorAction::action_workspace_4 },
    { "workspace_5", CompositorAction::action_workspace_5 },
    { "workspace_6", CompositorAction::action_workspace_6 },
    { "workspace_7", CompositorAction::action_workspace_7 },
    { "workspace_8", CompositorAction::action_workspace_8 },
    { "workspace_9", CompositorAction::action_workspace_9 },
    { "workspace_10", CompositorAction::action_workspace_10 },
    { "workspace_11", CompositorAction::action_workspace_11 },
    { "workspace_12", CompositorAction::action_workspace_12 },
    { "prev_workspace", CompositorAction::action_prev_workspace },
    { "next_workspace", CompositorAction::action_next_workspace },
    { "show_desktop", CompositorAction::action_show_desktop },
    { "open_multitask_view", CompositorAction::action_open_multitask_view },
    { "close_multitask_view", CompositorAction::action_close_multitask_view },
    { "toggle_multitask_view", CompositorAction::action_toggle_multitask_view },
    { "zoom_in", CompositorAction::action_zoom_in },
    { "zoom_out", CompositorAction::action_zoom_out },
    { "zoom_reset", CompositorAction::action_zoom_reset },
    { "toggle_fps_display", CompositorAction::action_toggle_fps_display },
    { "lockscreen", CompositorAction::action_lockscreen },
    { "shutdown_menu", CompositorAction::action_shutdown_menu },
    { "show_user_switch", CompositorAction::action_show_user_switch },
    { "shutdown_menu_power_off", CompositorAction::action_shutdown_menu_power_off },
    { "shutdown_menu_reboot", CompositorAction::action_shutdown_menu_reboot },
    { "shutdown_menu_suspend", CompositorAction::action_shutdown_menu_suspend },
    { "shutdown_menu_hibernate", CompositorAction::action_shutdown_menu_hibernate },
    { "shutdown_menu_log_out", CompositorAction::action_shutdown_menu_log_out },
};

static void printActions()
{
    printf("Available actions:\n");
    for (const auto &action : kActions)
        printf("  %-26s %u\n", action.name, action.value);
}

static bool resolveAction(const char *argument, uint32_t *value)
{
    for (const auto &action : kActions) {
        if (std::strcmp(action.name, argument) == 0) {
            *value = action.value;
            return true;
        }
    }

    char *end = nullptr;
    const long parsed = std::strtol(argument, &end, 10);
    if (end != argument && *end == '\0' && parsed > 0 && parsed < 1000) {
        *value = static_cast<uint32_t>(parsed);
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QGuiApplication app(argc, argv);

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <action-name|value|list>\n", argv[0]);
        printActions();
        return 1;
    }

    if (std::strcmp(argv[1], "list") == 0) {
        printActions();
        return 0;
    }

    uint32_t value = 0;
    if (!resolveAction(argv[1], &value)) {
        fprintf(stderr, "Unknown action: %s\n", argv[1]);
        printActions();
        return 1;
    }

    CompositorAction action;
    // The protocol has no bind feedback: if the compositor never advertises
    // the global (rejected as non-privileged), fail instead of hanging.
    QTimer::singleShot(3000, &action, [&action] {
        if (!action.isActive()) {
            fprintf(stderr, "treeland_compositor_action_v1 not advertised: "
                            "bind rejected or compositor does not support it\n");
            QCoreApplication::exit(1);
        }
    });
    QObject::connect(&action, &CompositorAction::activeChanged, &action, [&action, value] {
        if (!action.isActive()) {
            fprintf(stderr, "treeland_compositor_action_v1 not available: "
                            "the compositor rejected the bind (not privileged?)\n");
            QCoreApplication::exit(1);
            return;
        }
        printf("Triggering action %u\n", value);
        action.trigger(value);
        // Fire-and-forget: no reply event ever comes back, so flush the
        // request to the socket explicitly before exiting — a fixed delay
        // alone could leave the request buffered when the process dies.
        if (auto *integration = QtWaylandClient::QWaylandIntegration::instance())
            wl_display_flush(integration->display());
        QCoreApplication::exit(0);
    });

    return app.exec();
}

#include "main.moc"
