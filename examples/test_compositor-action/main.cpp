// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Test client for the treeland-compositor-action-unstable-v1 protocol.
//
// Usage:
//   test-compositor-action                    launch the GUI and click buttons
//   test-compositor-action list               print all action names and values
//   test-compositor-action <name|value>       trigger one action and exit, e.g.
//                                             ./test-compositor-action workspace_2
//                                             ./test-compositor-action show_desktop
//                                             ./test-compositor-action 23 (lockscreen)
//
// The protocol is fire-and-forget: the compositor sends no reply, so the
// client just triggers the action (and flushes the request to the socket).

#include "qwayland-treeland-compositor-action-unstable-v1.h"

#include <QApplication>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QtWaylandClient/QWaylandClientExtension>

#include <qguiapplication_platform.h>

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

// ---------------------------------------------------------------------------
// GUI
// ---------------------------------------------------------------------------

// Flushes the pending trigger() request to the compositor socket. The protocol
// is fire-and-forget (no reply event), so without an explicit flush the request
// could stay buffered in the client library.
static void flushWayland()
{
    if (auto *waylandApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>())
        wl_display_flush(waylandApp->display());
}

static QPushButton *addActionButton(CompositorAction *action,
                                    QLabel *status,
                                    const char *label,
                                    uint32_t value)
{
    auto *button = new QPushButton(QString::fromLatin1(label));
    QObject::connect(button, &QPushButton::clicked, button, [action, status, label, value] {
        action->trigger(value);
        flushWayland();
        status->setText(QStringLiteral("triggered %1 (%2)")
                            .arg(QString::fromLatin1(label))
                            .arg(value));
    });
    return button;
}

static QWidget *buildGui(CompositorAction *action)
{
    auto *window = new QWidget;
    window->setWindowTitle(QStringLiteral("Compositor Action Client"));
    window->setAttribute(Qt::WA_DeleteOnClose);

    auto *vbox = new QVBoxLayout(window);

    // Status line: the protocol is fire-and-forget (no reply event), so this
    // is the only feedback the user gets for a click.
    auto *status = new QLabel(QStringLiteral("connected to treeland_compositor_action_v1"));
    status->setWordWrap(true);
    vbox->addWidget(status);

    // ---- Workspace switching ----
    auto *workspaceBox = new QGroupBox(QStringLiteral("Workspace"));
    auto *workspaceGrid = new QGridLayout(workspaceBox);
    static const char *workspaceButtons[] = {
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
    };
    for (int i = 0; i < 12; ++i) {
        const uint32_t value = CompositorAction::action_workspace_1 + i;
        workspaceGrid->addWidget(addActionButton(action, status, workspaceButtons[i], value), i / 6, i % 6);
    }
    workspaceGrid->addWidget(addActionButton(action, status, "prev", CompositorAction::action_prev_workspace), 2, 0);
    workspaceGrid->addWidget(addActionButton(action, status, "next", CompositorAction::action_next_workspace), 2, 1);
    vbox->addWidget(workspaceBox);

    // ---- Show desktop / multitask ----
    auto *desktopBox = new QGroupBox(QStringLiteral("Show desktop / Multitask"));
    auto *desktopRow = new QHBoxLayout(desktopBox);
    desktopRow->addWidget(addActionButton(action, status, "show_desktop", CompositorAction::action_show_desktop));
    // Opening the multitask view covers this window, so the close button would
    // be unreachable. Open it, then auto-close 5s later so the client stays usable.
    auto *openButton = new QPushButton(QStringLiteral("open_multitask (auto-close 5s)"));
    QObject::connect(openButton, &QPushButton::clicked, openButton, [action, status] {
        action->trigger(CompositorAction::action_open_multitask_view);
        flushWayland();
        status->setText(QStringLiteral("opened multitask view — auto-close in 5s"));
        QTimer::singleShot(5000, status, [action, status] {
            action->trigger(CompositorAction::action_close_multitask_view);
            flushWayland();
            status->setText(QStringLiteral("closed multitask view"));
        });
    });
    desktopRow->addWidget(openButton);
    desktopRow->addWidget(addActionButton(action, status, "close_multitask", CompositorAction::action_close_multitask_view));
    desktopRow->addWidget(addActionButton(action, status, "toggle_multitask", CompositorAction::action_toggle_multitask_view));
    vbox->addWidget(desktopBox);

    // ---- Screen zoom ----
    auto *zoomBox = new QGroupBox(QStringLiteral("Screen zoom"));
    auto *zoomRow = new QHBoxLayout(zoomBox);
    zoomRow->addWidget(addActionButton(action, status, "zoom_in", CompositorAction::action_zoom_in));
    zoomRow->addWidget(addActionButton(action, status, "zoom_out", CompositorAction::action_zoom_out));
    zoomRow->addWidget(addActionButton(action, status, "zoom_reset", CompositorAction::action_zoom_reset));
    vbox->addWidget(zoomBox);

    // ---- System / session ----
    auto *sessionBox = new QGroupBox(QStringLiteral("System / Session"));
    auto *sessionRow = new QHBoxLayout(sessionBox);
    sessionRow->addWidget(addActionButton(action, status, "toggle_fps", CompositorAction::action_toggle_fps_display));
    sessionRow->addWidget(addActionButton(action, status, "lockscreen", CompositorAction::action_lockscreen));
    sessionRow->addWidget(addActionButton(action, status, "shutdown_menu", CompositorAction::action_shutdown_menu));
    sessionRow->addWidget(addActionButton(action, status, "user_switch", CompositorAction::action_show_user_switch));
    vbox->addWidget(sessionBox);

    // ---- Focused shutdown-menu variants ----
    auto *shutdownBox = new QGroupBox(QStringLiteral("Shutdown menu (focused variant)"));
    auto *shutdownGrid = new QGridLayout(shutdownBox);
    shutdownGrid->addWidget(addActionButton(action, status, "power_off", CompositorAction::action_shutdown_menu_power_off), 0, 0);
    shutdownGrid->addWidget(addActionButton(action, status, "reboot", CompositorAction::action_shutdown_menu_reboot), 0, 1);
    shutdownGrid->addWidget(addActionButton(action, status, "suspend", CompositorAction::action_shutdown_menu_suspend), 0, 2);
    shutdownGrid->addWidget(addActionButton(action, status, "hibernate", CompositorAction::action_shutdown_menu_hibernate), 1, 0);
    shutdownGrid->addWidget(addActionButton(action, status, "log_out", CompositorAction::action_shutdown_menu_log_out), 1, 1);
    vbox->addWidget(shutdownBox);

    return window;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [<action-name|value>|list]\n", argv[0]);
        printActions();
        return 1;
    }

    if (argc == 2 && std::strcmp(argv[1], "list") == 0) {
        printActions();
        return 0;
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

    QObject::connect(&action, &CompositorAction::activeChanged, &action, [&action, argc, argv] {
        if (!action.isActive()) {
            fprintf(stderr, "treeland_compositor_action_v1 not available: "
                            "the compositor rejected the bind (not privileged?)\n");
            QCoreApplication::exit(1);
            return;
        }

        if (argc == 2) {
            // One-shot CLI mode: trigger a single action and exit.
            uint32_t value = 0;
            if (!resolveAction(argv[1], &value)) {
                fprintf(stderr, "Unknown action: %s\n", argv[1]);
                printActions();
                QCoreApplication::exit(1);
                return;
            }
            printf("Triggering action %u\n", value);
            action.trigger(value);
            flushWayland();
            QCoreApplication::exit(0);
            return;
        }

        buildGui(&action)->show();
    });

    return app.exec();
}

#include "main.moc"
