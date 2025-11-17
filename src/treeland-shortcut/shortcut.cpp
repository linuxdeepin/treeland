// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcut.h"

#include <systemd/sd-daemon.h>

#include <QApplication>
#include <QDBusInterface>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(treelandShortcut, "daemon.shortcut", QtDebugMsg);

static const QMap<QString, ShortcutV2::action> ActionMap = {
    {"Notify", ShortcutV2::action_notify},
    {"Workspace1", ShortcutV2::action_workspace_1},
    {"Workspace2", ShortcutV2::action_workspace_2},
    {"Workspace3", ShortcutV2::action_workspace_3},
    {"Workspace4", ShortcutV2::action_workspace_4},
    {"Workspace5", ShortcutV2::action_workspace_5},
    {"Workspace6", ShortcutV2::action_workspace_6},
    {"PrevWorkspace", ShortcutV2::action_prev_workspace},
    {"NextWorkspace", ShortcutV2::action_next_workspace},
    {"ShowDesktop", ShortcutV2::action_show_desktop},
    {"Maximize", ShortcutV2::action_maximize},
    {"CancelMaximize", ShortcutV2::action_cancel_maximize},
    {"MoveWindow", ShortcutV2::action_move_window},
    {"CloseWindow", ShortcutV2::action_close_window},
    {"ShowWindowMenu", ShortcutV2::action_show_window_menu},
    {"OpenMultitaskView", ShortcutV2::action_open_multitask_view},
    {"CloseMultitaskView", ShortcutV2::action_close_multitask_view},
    {"ToggleMultitaskView", ShortcutV2::action_toggle_multitask_view},
    {"ToggleFpsDisplay", ShortcutV2::action_toggle_fps_display},
    {"Lockscreen", ShortcutV2::action_lockscreen},
    {"ShutdownMenu", ShortcutV2::action_shutdown_menu},
    {"Quit", ShortcutV2::action_quit},
    {"TaskswitchEnter", ShortcutV2::action_taskswitch_enter},
    {"TaskswitchNext", ShortcutV2::action_taskswitch_next},
    {"TaskswitchPrev", ShortcutV2::action_taskswitch_prev},
    {"TaskswitchSameAppNext", ShortcutV2::action_taskswitch_sameapp_next},
    {"TaskswitchSameAppPrev", ShortcutV2::action_taskswitch_sameapp_prev},
};

static const QMap<QString, ShortcutV2::keybind_mode> KeybindModeMap = {
    {"KeyRelease", ShortcutV2::keybind_mode_key_release},
    {"KeyPress", ShortcutV2::keybind_mode_key_press},
    {"KeyPressRepeat", ShortcutV2::keybind_mode_key_press_repeat},
};

static const QMap<QString, ShortcutV2::direction> GestureDirectionMap = {
    {"SwipeUp", ShortcutV2::direction_up},
    {"SwipeDown", ShortcutV2::direction_down},
    {"SwipeLeft", ShortcutV2::direction_left},
    {"SwipeRight", ShortcutV2::direction_right},
};

static const QMap<QString, QString> SpecialKeyMap = {
    { "minus", "-" },      { "equal", "=" },     { "brackertleft", "[" }, { "breckertright", "]" },
    { "backslash", "\\" }, { "semicolon", ";" }, { "apostrophe", "'" },   { "comma", "," },
    { "period", "." },     { "slash", "/" },     { "grave", "`" },
};

static const QMap<QString, QString> SpecialRequireShiftKeyMap = {
    { "exclam", "!" },    { "at", "@" },          { "numbersign", "#" }, { "dollar", "$" },
    { "percent", "%" },   { "asciicircum", "^" }, { "ampersand", "&" },  { "asterisk", "*" },
    { "parenleft", "(" }, { "parenright", ")" },  { "underscore", "_" }, { "plus", "+" },
    { "braceleft", "{" }, { "braceright", "}" },  { "bar", "|" },        { "colon", ":" },
    { "quotedbl", "\"" }, { "less", "<" },        { "greater", ">" },    { "question", "?" },
    { "asciitilde", "~" }
};

// from dtkcore util
QString getAppIdFromAbsolutePath(const QString &path)
{
    static QString desktopSuffix{ u8".desktop" };
    const auto &appDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    if (!path.endsWith(desktopSuffix)
        || !std::any_of(appDirs.cbegin(), appDirs.constEnd(), [&path](const QString &dir) {
               return path.startsWith(dir);
           })) {
        return {};
    }

    auto tmp = path.chopped(desktopSuffix.size());
    auto components = tmp.split(QDir::separator(), Qt::SkipEmptyParts);
    auto location = std::find(components.cbegin(), components.cend(), "applications");
    if (location == components.cend()) {
        return {};
    }

    auto appId = QStringList{ location + 1, components.cend() }.join('-');
    return appId;
}

QString escapeToObjectPath(const QString &str)
{
    if (str.isEmpty()) {
        return "_";
    }

    auto ret = str;
    QRegularExpression re{ R"([^a-zA-Z0-9])" };
    auto matcher = re.globalMatch(ret);
    while (matcher.hasNext()) {
        auto replaceList = matcher.next().capturedTexts();
        replaceList.removeDuplicates();
        for (const auto &c : replaceList) {
            auto hexStr = QString::number(static_cast<uint>(c.front().toLatin1()), 16);
            ret.replace(c, QString{ R"(_%1)" }.arg(hexStr));
        }
    }
    return ret;
}

QString transFromDaemonAccelStr(const QString &accelStr)
{
    if (accelStr.isEmpty()) {
        return accelStr;
    }

    QString str(accelStr);

    str.remove("<").replace(">", "+").replace("Control", "Ctrl").replace("Super", "Meta");

    for (auto it = SpecialKeyMap.constBegin(); it != SpecialKeyMap.constEnd(); ++it) {
        QString origin(str);
        str.replace(it.key(), it.value());
        if (str != origin) {
            return str;
        }
    }

    for (auto it = SpecialRequireShiftKeyMap.constBegin();
         it != SpecialRequireShiftKeyMap.constEnd();
         ++it) {
        QString origin(str);
        str.replace(it.key(), it.value());
        if (str != origin) {
            return str.remove("Shift+");
        }
    }

    return str;
}

void ShortcutV2::treeland_shortcut_manager_v2_commit_success()
{
    Q_EMIT commitStatus(true);
}

void ShortcutV2::treeland_shortcut_manager_v2_commit_failure(const QString &name, uint32_t error)
{
    qCWarning(treelandShortcut) << "Commit failure for shortcut:" << name << ", error code:" << error;
    Q_EMIT commitStatus(false);
}

void ShortcutV2::treeland_shortcut_manager_v2_activated(const QString &name, uint32_t repeat)
{
    Q_EMIT activated(name, repeat);
}

ShortcutV2::ShortcutV2()
    : QWaylandClientExtensionTemplate<ShortcutV2>(1)
{
    connect(this, &ShortcutV2::activeChanged, this, [this] {
        qCDebug(treelandShortcut) << "isActive:" << isActive();

        if (isActive()) {
            acquire();

            // NOTE: support of dynamic update shortcuts from ini file is temporarily dropped
            // auto updateShortcuts = [this, customIni] {
                // QSettings custom(customIni, QSettings::IniFormat);
                // for (auto group : custom.childGroups()) {
                //     const QString &action =
                //         custom.value(QString("%1/Action").arg(group)).toString();
                //     const QString &accels = transFromDaemonAccelStr(
                //         custom.value(QString("%1/Accels").arg(group)).toString());
                //     ShortcutContext *context =
                //         new ShortcutContext(register_shortcut_context(accels));
                //     connect(context, &ShortcutContext::shortcutHappened, this, [action] {
                //         QProcess::startDetached(action);
                //     });
                //     m_customShortcuts.emplace_back(context);
                // }
            // };

            // QFileSystemWatcher *watcher = new QFileSystemWatcher({ customIni }, this);
            // connect(watcher, &QFileSystemWatcher::fileChanged, this, [updateShortcuts] {
            //     updateShortcuts();
            // });

            // updateShortcuts();

            QDir dir(TREELAND_DATA_DIR "/shortcuts");
            for (auto d : dir.entryInfoList(QDir::Filter::Files)) {
                qCInfo(treelandShortcut) << "Load shortcut:" << d.filePath();
                auto shortcut = new Shortcut(d.filePath(), d.fileName());
                shortcut->registerForManager(this);
            }
        }
    });
}

Shortcut::Shortcut(const QString &path, const QString &name)
    : m_settings(QSettings(path, QSettings::IniFormat))
    , m_shortcutName(name)
{
}

Shortcut::~Shortcut()
{
    Q_EMIT before_destroy();
}

void Shortcut::exec()
{
    const QString &type = m_settings.value("Shortcut/Type").toString();

    if (type == "Exec") {
        const QString &typeExec = m_settings.value("Type.Exec/Exec").toString();
        const QString &typeArgs = m_settings.value("Type.Exec/Args").toString();
        QProcess::startDetached(typeExec, typeArgs.split(" "));
    }

    if (type == "DBus") {
        const QString &service = m_settings.value("Type.DBus/Service").toString();
        const QString &path = m_settings.value("Type.DBus/Path").toString();
        const QString &interface = m_settings.value("Type.DBus/Interface").toString();
        const QString &method = m_settings.value("Type.DBus/Method").toString();

        QDBusInterface dbus(service, path, interface);
        dbus.asyncCall(method);
    }

    if (type == "Action") { }

    if (type == "Application") {
        const QString &service = u8"org.desktopspec.ApplicationManager1";
        const QString &prefixPath = u8"/org/desktopspec/ApplicationManager1/";
        const QString &interface = u8"org.desktopspec.ApplicationManager1.Application";
        const QString &appPath = m_settings.value("Type.Application/App").toString();
        const QString &appId = getAppIdFromAbsolutePath(appPath);
        const QString &objectPath = prefixPath + escapeToObjectPath(appId);

        QDBusInterface dbus(service, objectPath, interface);
        dbus.asyncCall(u8"Launch", "", QStringList{}, QVariantMap{});
    }
}

void Shortcut::registerForManager(ShortcutV2 *manager)
{
    const QString &type = m_settings.value("Shortcut/Type").toString();
    ShortcutV2::action action = ShortcutV2::action_notify;
    if (type == "Action") {
        auto actionStr = m_settings.value("Type.Action/Action").toString();
        if (ActionMap.contains(actionStr)) {
            action = ActionMap.value(actionStr);
        } else {
            qCWarning(treelandShortcut) << "Unknown action:" << actionStr
                                        << "for shortcut:" << m_shortcutName
                                        << ", shortcut registration skipped.";
            return;
        }
    }

    auto shortcut = m_settings.value("Shortcut/Shortcut").toString();
    if (!shortcut.isEmpty()) {
        auto keybindName = QString("%1_key").arg(m_shortcutName);

        ShortcutV2::keybind_mode mode = ShortcutV2::keybind_mode_key_press;
        auto modeStr = m_settings.value("Shortcut/KeybindMode").toString();
        if (KeybindModeMap.contains(modeStr)) {
            mode = KeybindModeMap.value(modeStr);
        }
        manager->bind_key(keybindName, shortcut, mode, action);
        m_registeredBindings.append(keybindName);
    }

    do {
        const auto gestureName = QString("%1_gesture").arg(m_shortcutName);
        // format: 3FingerSwipeUp, 2FingerSwipeRight, 4FingerHold, etc.
        auto gestureStr = m_settings.value("Shortcut/Gesture").toString();
        if (gestureStr.isEmpty()) {
            break;
        }

        static const QRegularExpression regex(QStringLiteral("^(\\d+)Finger(.+)$"));
        const auto match = regex.match(gestureStr);
        if (!match.hasMatch()) {
            qCWarning(treelandShortcut) << "Invalid gesture format:" << gestureStr;
            break;
        }

        const int fingerCount = match.captured(1).toInt();
        const QString movement = match.captured(2);
        if (fingerCount < 2 || fingerCount > 4) {
            // unsupported by libinput
            qCWarning(treelandShortcut) << "Unsupported finger count:" << fingerCount
                                        << "for gesture:" << gestureStr;
            break;
        }
        if (movement == "Hold") {
            manager->bind_hold_gesture(gestureName, fingerCount, action);
        } else {
            if (!GestureDirectionMap.contains(movement)) {
                qCWarning(treelandShortcut) << "Unknown gesture movement:" << movement
                                            << "for gesture:" << gestureStr;
                break;
            }
            const auto direction = GestureDirectionMap.value(movement);
            manager->bind_swipe_gesture(gestureName, fingerCount, direction, action);
        }
    } while (false);

    QEventLoop loop;
    QObject::connect(manager, &ShortcutV2::commitStatus, this, [this, manager, type, &loop](bool success) {
        loop.quit();
        if (!success) {
            qCWarning(treelandShortcut) << "Shortcut commit failed for shortcut:" << m_shortcutName;
            m_registeredBindings.clear();
        } else {
            // unbind on destroy
            connect(this, &Shortcut::before_destroy, [this, manager]() {
                for (const auto &name : std::as_const(m_registeredBindings)) {
                    manager->unbind(name);
                }
            });
            if (type == "Action") {
                // compositor action does not need client handling
                return;
            }
            connect(manager, &ShortcutV2::activated, this, [this](const QString &name) {
                for (const auto &registeredName : std::as_const(m_registeredBindings)) {
                    if (name == registeredName) {
                        this->exec();
                        break;
                    }
                }
            });
        }
    }, Qt::SingleShotConnection);

    manager->commit();
    loop.exec();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    new ShortcutV2;

    sd_notify(0, "READY=1");

    return app.exec();
}
