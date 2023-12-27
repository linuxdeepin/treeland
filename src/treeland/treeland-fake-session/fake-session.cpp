// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "fake-session.h"

#include "shortcutmanager.h"
#include "Constants.h"

#include <QDebug>
#include <QObject>
#include <QProcess>
#include <QWindow>
#include <QWidget>
#include <QBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QDBusInterface>
#include <QtWaylandClient/QWaylandClientExtension>
#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QFileSystemWatcher>

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

namespace Protocols {
ExtForeignToplevelList::ExtForeignToplevelList()
    : QWaylandClientExtensionTemplate<ExtForeignToplevelList>(1)
{
}

void ExtForeignToplevelList::ext_foreign_toplevel_list_v1_toplevel(struct ::ext_foreign_toplevel_handle_v1 *toplevel)
{
    ExtForeignToplevelHandle *handle = new ExtForeignToplevelHandle(toplevel);
    emit newToplevel(handle);

    qDebug() << Q_FUNC_INFO << "toplevel create!!!!!!";
}

void ExtForeignToplevelList::ext_foreign_toplevel_list_v1_finished()
{

}

ExtForeignToplevelHandle::ExtForeignToplevelHandle(struct ::ext_foreign_toplevel_handle_v1 *object)
    : QWaylandClientExtensionTemplate<ExtForeignToplevelHandle>(1)
    , QtWayland::ext_foreign_toplevel_handle_v1(object)
{}

void ExtForeignToplevelHandle::ext_foreign_toplevel_handle_v1_app_id(const QString &app_id)
{
    emit appIdChanged(app_id);
}

void ExtForeignToplevelHandle::ext_foreign_toplevel_handle_v1_closed()
{
    qDebug() << Q_FUNC_INFO << "toplevel closed!!!!!!";
}

void ExtForeignToplevelHandle::ext_foreign_toplevel_handle_v1_identifier(const QString &identifier)
{
    qDebug() << Q_FUNC_INFO << identifier;
}

ForeignToplevelManager::ForeignToplevelManager()
    : QWaylandClientExtensionTemplate<ForeignToplevelManager>(1)
{

}

void ForeignToplevelManager::ztreeland_foreign_toplevel_manager_v1_toplevel(struct ::ztreeland_foreign_toplevel_handle_v1 *toplevel)
{
    ForeignToplevelHandle* handle = new ForeignToplevelHandle(toplevel);
    emit newForeignToplevelHandle(handle);

    qDebug() << Q_FUNC_INFO << "toplevel create!!!!!!";
}

ForeignToplevelHandle::ForeignToplevelHandle(struct ::ztreeland_foreign_toplevel_handle_v1 *object)
    : QWaylandClientExtensionTemplate<ForeignToplevelHandle>(1)
    , QtWayland::ztreeland_foreign_toplevel_handle_v1(object)
    , m_pid(-1)
{}

void ForeignToplevelHandle::ztreeland_foreign_toplevel_handle_v1_app_id([[maybe_unused]] const QString &app_id)
{
}

void ForeignToplevelHandle::ztreeland_foreign_toplevel_handle_v1_pid(uint32_t pid)
{
    m_pid = pid;
}

void ForeignToplevelHandle::ztreeland_foreign_toplevel_handle_v1_done()
{
    emit pidChanged(m_pid);
}
void ForeignToplevelHandle::ztreeland_foreign_toplevel_handle_v1_closed()
{

}

ShortcutManager::ShortcutManager()
    : QWaylandClientExtensionTemplate<ShortcutManager>(1)
{

}

ShortcutContext::ShortcutContext(struct ::treeland_shortcut_context_v1 *object)
    : QWaylandClientExtensionTemplate<ShortcutContext>(1)
    , QtWayland::treeland_shortcut_context_v1(object)
{

}

ShortcutContext::~ShortcutContext() {
    destroy();
}

void ShortcutContext::treeland_shortcut_context_v1_shortcut()
{
    emit shortcutHappended();
}

PersonalizationManager::PersonalizationManager()
    : QWaylandClientExtensionTemplate<PersonalizationManager>(1)
{

}

PersonalizationWindow::PersonalizationWindow(struct ::personalization_window_context_v1 *object)
    : QWaylandClientExtensionTemplate<PersonalizationWindow>(1)
    , QtWayland::personalization_window_context_v1(object)
{

}

PersonalizationWallpaper::PersonalizationWallpaper(struct ::personalization_wallpaper_context_v1 *object)
    : QWaylandClientExtensionTemplate<PersonalizationWallpaper>(1)
    , QtWayland::personalization_wallpaper_context_v1(object)
{

}

void PersonalizationWallpaper::personalization_wallpaper_context_v1_wallpapers(const QString &metadata)
{
    qDebug() << "=========================================== " << metadata;
}
}

static const QMap<QString, QString> SpecialKeyMap = {
    {"minus", "-"}, {"equal", "="}, {"brackertleft", "["}, {"breckertright", "]"},
    {"backslash", "\\"}, {"semicolon", ";"}, {"apostrophe", "'"}, {"comma", ","},
    {"period", "."}, {"slash", "/"}, {"grave", "`"},
};

static const QMap<QString, QString> SpecialRequireShiftKeyMap = {
    {"exclam", "!"}, {"at", "@"}, {"numbersign", "#"}, {"dollar", "$"},
    {"percent", "%"}, {"asciicircum", "^"}, {"ampersand", "&"}, {"asterisk", "*"},
    {"parenleft", "("}, {"parenright", ")"}, {"underscore", "_"}, {"plus", "+"},
    {"braceleft", "{"}, {"braceright", "}"}, {"bar", "|"}, {"colon", ":"},
    {"quotedbl", "\""}, {"less", "<"}, {"greater", ">"}, {"question", "?"},
    {"asciitilde", "~"}
};

QString transFromDaemonAccelStr(const QString &accelStr)
{
    if (accelStr.isEmpty()) {
        return accelStr;
    }

    QString str(accelStr);

    str.remove("<")
            .replace(">", "+")
            .replace("Control", "Ctrl")
            .replace("Super", "Meta");

    for (auto it = SpecialKeyMap.constBegin(); it != SpecialKeyMap.constEnd(); ++it) {
        QString origin(str);
        str.replace(it.key(), it.value());
        if (str != origin) {
            return str;
        }
    }

    for (auto it = SpecialRequireShiftKeyMap.constBegin(); it != SpecialRequireShiftKeyMap.constEnd(); ++it) {
        QString origin(str);
        str.replace(it.key(), it.value());
        if (str != origin) {
            return str.remove("Shift+");
        }
    }

    return str;
}

static int click_state = 0;
FakeSession::FakeSession(int argc, char* argv[])
    : QApplication(argc, argv)
    , m_personalzationManger(new Protocols::PersonalizationManager)
    , m_shortcutManager(new Protocols::ShortcutManager)
    , m_toplevelManager(new Protocols::ForeignToplevelManager)
    , m_extForeignToplevelList(new Protocols::ExtForeignToplevelList)
{
    connect(m_shortcutManager, &Protocols::ShortcutManager::activeChanged, this, [this] {
        qDebug() << m_shortcutManager->isActive();

        if (m_shortcutManager->isActive()) {
            // TODO: Use a converter
            const QString configDir = QString("%1/deepin/dde-daemon/keybinding/").arg(QStandardPaths::standardLocations(QStandardPaths::ConfigLocation).first());
            const QString customIni = QString("%1/custom.ini").arg(configDir);

            auto updateShortcuts = [this, customIni] {
                for (auto *context : m_customShortcuts) {
                    delete context;
                }

                m_customShortcuts.clear();

                QSettings custom(customIni, QSettings::IniFormat);
                for (auto group : custom.childGroups()) {
                    const QString &action = custom.value(QString("%1/Action").arg(group)).toString();
                    const QString &accels = transFromDaemonAccelStr(custom.value(QString("%1/Accels").arg(group)).toString());
                    Protocols::ShortcutContext* context = new Protocols::ShortcutContext(m_shortcutManager->register_shortcut_context(accels));
                    m_customShortcuts.push_back(context);
                    connect(context, &Protocols::ShortcutContext::shortcutHappended, this, [action] {
                        QProcess::startDetached(action);
                    });
                }
            };

            QFileSystemWatcher *watcher = new QFileSystemWatcher({customIni}, this);
            connect(watcher, &QFileSystemWatcher::fileChanged, this, [updateShortcuts] {
                updateShortcuts();
            });

            updateShortcuts();

            QDir dir(TREELAND_DATA_DIR"/shortcuts");
            for (auto d : dir.entryInfoList(QDir::Filter::Files)) {
                auto shortcut = new Shortcut(d.filePath());
                Protocols::ShortcutContext* context = new Protocols::ShortcutContext(m_shortcutManager->register_shortcut_context(shortcut->shortcut()));
                connect(context, &Protocols::ShortcutContext::shortcutHappended, this, [shortcut] {
                    shortcut->exec();
                });
            }
        }
    });

    connect(m_personalzationManger, &Protocols::PersonalizationManager::activeChanged, this, [this] {
        qDebug() << "personalzation manager" <<  m_personalzationManger->isActive();

        if (m_personalzationManger->isActive()) {
            QWidget *widget = new QWidget;
            widget->setAttribute(Qt::WA_TranslucentBackground);
            widget->setWindowFlags(Qt::FramelessWindowHint); // 可选，去除窗口边框
            widget->resize(640, 480);

            QHBoxLayout* layout = new QHBoxLayout;

            QPushButton *click_button = new QPushButton("Click Me");
            QPushButton *set_button = new QPushButton("Set Wallpaper");
            QPushButton *get_button = new QPushButton("Get Wallpaper");

            layout->addWidget(click_button);
            layout->addWidget(set_button);
            layout->addWidget(get_button);

            widget->setLayout(layout);
            widget->show();

            QWindow *window = widget->windowHandle();

            if (window && window->handle()) {
                QtWaylandClient::QWaylandWindow *waylandWindow =
                    static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());

                struct wl_surface *surface = waylandWindow->wlSurface();
                if (surface) {
                    Protocols::PersonalizationWindow* window_context = new Protocols::PersonalizationWindow(m_personalzationManger->get_window_context(surface));

                    QObject::connect(click_button, &QPushButton::clicked, [window_context](){
                        click_state = !click_state;
                        window_context->set_background_type(click_state);
                        qDebug() << "===========background state: ==========" << click_state;
                    });
                }

                Protocols::PersonalizationWallpaper* wallpaper_context = new Protocols::PersonalizationWallpaper(m_personalzationManger->get_wallpaper_context());
                QObject::connect(set_button, &QPushButton::clicked, [wallpaper_context](){
                    qDebug() << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx set user wallpaper";
                    QFileDialog fileDialog;
                    fileDialog.setFileMode(QFileDialog::ExistingFile);
                    fileDialog.setNameFilter("Images (*.png *.jpg *.bmp *.gif)");

                    if (fileDialog.exec() == QDialog::Accepted) {
                        // 获取用户选择的文件路径
                        QString selectedFilePath = fileDialog.selectedFiles().first();
                        QFile file(selectedFilePath);
                        if (file.open(QIODevice::ReadOnly)) {
                            QJsonObject json;
                            json.insert("Group", "system");
                            json.insert("ImagePath", selectedFilePath);
                            json.insert("FillMode", 1);
                            json.insert("Output", "eDP-1");
                            json.insert("CurrentIndex", 4);

                            QJsonDocument json_doc(json);;
                            wallpaper_context->set_wallpaper(file.handle(), json_doc.toJson(QJsonDocument::Compact));
                        }
                    }
                });

                QObject::connect(get_button, &QPushButton::clicked, [wallpaper_context](){
                    wallpaper_context->get_wallpapers();
                });
            }
        }
    });

    connect(m_toplevelManager, &Protocols::ForeignToplevelManager::newForeignToplevelHandle, this, [this](Protocols::ForeignToplevelHandle *handle) {
        connect(handle, &Protocols::ForeignToplevelHandle::pidChanged, this, [](pid_t pid) {
            qDebug() << "toplevel pid: " << pid;
        });
    });

    connect(m_extForeignToplevelList, &Protocols::ExtForeignToplevelList::newToplevel, this, [this](Protocols::ExtForeignToplevelHandle *handle) {
        connect(handle, &Protocols::ExtForeignToplevelHandle::appIdChanged, this, [](const QString &appId) {
            qDebug() << "toplevel appid: " << appId;
        });
    });

    emit m_shortcutManager->activeChanged();

    QProcess::startDetached("dde-shell", {"-p", "org.deepin.ds.dock"});

    QDBusInterface systemd("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.systemd1.Manager");
    systemd.call("UnsetEnvironment", QStringList{"DISPLAY", "WAYLAND_DISPLAY", "XDG_SESSION_TYPE"});
    systemd.call("SetEnvironment", QStringList{
                                       QString("DISPLAY=%1").arg(qgetenv("DISPLAY")),
                                       QString("WAYLAND_DISPLAY=%1").arg(qgetenv("WAYLAND_DISPLAY")),
                                       QString("XDG_SESSION_TYPE=%1").arg(qgetenv("XDG_SESSION_TYPE")),
                                       QString("XDG_CURRENT_DESKTOP=%1").arg(qgetenv("XDG_CURRENT_DESKTOP")),
                                   }
    );
}

int main (int argc, char *argv[]) {
    FakeSession helper(argc, argv);

    return helper.exec();
}
