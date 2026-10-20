// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Simple GUI test application for the Treeland Shortcut Manager V3 protocol.
//
// A window lists a set of hardcoded default shortcuts (the same compositor
// actions shipped under /usr/share/dsg/configs/org.deepin.dde.keybinding).
// Ticking a checkbox binds the shortcut with the compositor; unticking it
// unbinds the shortcut. There is no JSON input: the shortcuts are hardcoded
// below so the tool can be run without any external configuration.

#include "qwayland-treeland-shortcut-manager-unstable-v3.h"

#include <QApplication>
#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QWaylandClientExtension>
#include <QWidget>

#include <cstdint>
#include <iterator>

// Wraps treeland_shortcut_manager_v3 and re-emits its events as Qt signals.
class ShortcutManagerV3
    : public QWaylandClientExtensionTemplate<ShortcutManagerV3>
    , public QtWayland::treeland_shortcut_manager_v3
{
    Q_OBJECT
public:
    explicit ShortcutManagerV3()
        : QWaylandClientExtensionTemplate<ShortcutManagerV3>(1)
    {
    }

    void bindShortcut(const QString &name, const QString &key, uint32_t flags, uint32_t action)
    {
        bind_key(name, key, flags, action);
    }

    void unbindShortcut(const QString &name)
    {
        unbind(name);
    }

Q_SIGNALS:
    void activated(const QString &name, uint32_t flags);
    void bindFailed(const QString &name, uint32_t error);

protected:
    void treeland_shortcut_manager_v3_activated(const QString &name, uint32_t flags) override
    {
        emit activated(name, flags);
    }

    void treeland_shortcut_manager_v3_bind_failure(const QString &name, uint32_t error) override
    {
        emit bindFailed(name, error);
    }
};

namespace {

using Action = QtWayland::treeland_shortcut_manager_v3::action;
using Flag = QtWayland::treeland_shortcut_manager_v3::keybind_flag;

struct ShortcutDef
{
    const char *name;
    const char *key;
    Action action;
    uint32_t flags;
};

// Hardcoded defaults mirroring the compositor actions shipped in
// /usr/share/dsg/configs/org.deepin.dde.keybinding (triggerType == 3).
// Action values here are already the V3 protocol enum values.
const ShortcutDef kShortcuts[] = {
    { "show-desktop",         "Meta+D",            Action::action_show_desktop,          Flag::keybind_flag_key_release },
    { "maximize",             "Meta+Up",           Action::action_maximize,               Flag::keybind_flag_key_release },
    { "cancel-maximize",      "Meta+Down",         Action::action_cancel_maximize,        Flag::keybind_flag_key_release },
    { "minimize",             "Meta+N",            Action::action_minimize,               Flag::keybind_flag_key_release },
    { "close-window",         "Alt+F4",            Action::action_close_window,           Flag::keybind_flag_key_release },
    { "move-window",          "Alt+F7",            Action::action_move_window,            Flag::keybind_flag_key_release },
    { "resize-window",        "Alt+F8",            Action::action_resize_window,          Flag::keybind_flag_key_release },
    { "show-window-menu",     "Alt+Space",         Action::action_show_window_menu,       Flag::keybind_flag_key_release },
    { "toggle-multitaskview", "Meta+S",            Action::action_toggle_multitask_view,  Flag::keybind_flag_key_release },
    { "toggle-fpsdisplay",    "Meta+F11",          Action::action_toggle_fps_display,     Flag::keybind_flag_key_release },
    { "workspace-1",          "Meta+1",            Action::action_workspace_1,            Flag::keybind_flag_key_release },
    { "workspace-2",          "Meta+2",            Action::action_workspace_2,            Flag::keybind_flag_key_release },
    { "workspace-3",          "Meta+3",            Action::action_workspace_3,            Flag::keybind_flag_key_release },
    { "workspace-4",          "Meta+4",            Action::action_workspace_4,            Flag::keybind_flag_key_release },
    { "prev-workspace",       "Meta+Ctrl+Left",    Action::action_prev_workspace,         Flag::keybind_flag_key_release },
    { "next-workspace",       "Meta+Ctrl+Right",   Action::action_next_workspace,         Flag::keybind_flag_key_release },
    { "tile-left",            "Meta+Left",         Action::action_tile_left,              Flag::keybind_flag_key_release },
    { "tile-right",           "Meta+Right",        Action::action_tile_right,             Flag::keybind_flag_key_release },
    { "taskswitch-next",      "Alt+Tab",           Action::action_taskswitch_next,        Flag::keybind_flag_key_press | Flag::keybind_flag_repeat },
    { "taskswitch-prev",      "Alt+Shift+Tab",     Action::action_taskswitch_prev,        Flag::keybind_flag_key_press | Flag::keybind_flag_repeat },
    { "zoom-in",              "Meta+=",            Action::action_zoom_in,                Flag::keybind_flag_key_release },
    { "zoom-out",             "Meta+-",            Action::action_zoom_out,               Flag::keybind_flag_key_release },
    { "zoom-reset",           "Meta+0",            Action::action_zoom_reset,             Flag::keybind_flag_key_release },
};

} // namespace

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);

    ShortcutManagerV3 manager;

    QWidget window;
    window.setWindowTitle(QStringLiteral("Shortcut Manager V3 Test"));
    window.resize(520, 720);

    auto *layout = new QVBoxLayout(&window);

    auto *statusLabel = new QLabel(&window);
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);

    auto *log = new QPlainTextEdit(&window);
    log->setReadOnly(true);
    layout->addWidget(log, 1);

    // One checkbox per hardcoded shortcut, laid out in two columns.
    // Checked == bound; all shortcuts start checked (bound) by default.
    const int shortcutCount = static_cast<int>(std::size(kShortcuts));
    auto *grid = new QGridLayout;
    constexpr int columns = 2;
    QList<QCheckBox *> boxes;
    for (int i = 0; i < shortcutCount; ++i) {
        const ShortcutDef &def = kShortcuts[i];
        auto *box = new QCheckBox(
            QStringLiteral("%1  (%2)").arg(QString::fromLatin1(def.name), QString::fromLatin1(def.key)),
            &window);
        box->setChecked(true);
        grid->addWidget(box, i / columns, i % columns);
        boxes.append(box);
    }
    layout->addLayout(grid);

    // (Re)apply every checkbox to the compositor. Used when the protocol
    // becomes active so previously checked shortcuts are re-bound after a
    // disconnect/reconnect of the extension.
    const auto applyBindings = [&]() {
        for (int i = 0; i < shortcutCount; ++i) {
            const ShortcutDef &def = kShortcuts[i];
            if (boxes.at(i)->isChecked())
                manager.bindShortcut(QString::fromLatin1(def.name),
                                     QString::fromLatin1(def.key),
                                     def.flags,
                                     def.action);
            else
                manager.unbindShortcut(QString::fromLatin1(def.name));
        }
    };

    QObject::connect(&manager, &ShortcutManagerV3::activeChanged, &window, [&]() {
        const bool active = manager.isActive();
        statusLabel->setText(active
                                 ? QStringLiteral("Protocol active — acquired, bindings applied.")
                                 : QStringLiteral("Protocol inactive."));
        if (!active)
            return;
        manager.acquire();
        applyBindings();
    });

    for (int i = 0; i < shortcutCount; ++i) {
        const ShortcutDef &def = kShortcuts[i];
        QObject::connect(boxes.at(i), &QCheckBox::toggled, &window, [&, i](bool checked) {
            if (checked)
                manager.bindShortcut(QString::fromLatin1(def.name),
                                     QString::fromLatin1(def.key),
                                     def.flags,
                                     def.action);
            else
                manager.unbindShortcut(QString::fromLatin1(def.name));
            log->appendPlainText(QStringLiteral("%1 %2")
                                     .arg(checked ? QStringLiteral("bound") : QStringLiteral("unbound"),
                                          QString::fromLatin1(def.name)));
        });
    }

    QObject::connect(&manager, &ShortcutManagerV3::activated, &window,
                     [log](const QString &name, uint32_t flags) {
                         log->appendPlainText(
                             QStringLiteral("activated: %1 (flags=0x%2)")
                                 .arg(name, QString::number(flags, 16)));
                     });

    QObject::connect(&manager, &ShortcutManagerV3::bindFailed, &window,
                     [log](const QString &name, uint32_t error) {
                         log->appendPlainText(
                             QStringLiteral("bind failure: %1 (error=%2)")
                                 .arg(name, QString::number(error)));
                     });

    window.show();
    return app.exec();
}

#include "main.moc"
