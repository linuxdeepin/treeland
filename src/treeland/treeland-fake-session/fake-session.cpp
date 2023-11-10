// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "fake-session.h"

#include <QDebug>
#include <QObject>
#include <QProcess>
#include <QWindow>
#include <QTimer>
#include <QtGui/qpa/qplatformnativeinterface.h>

#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

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

void ForeignToplevelHandle::ztreeland_foreign_toplevel_handle_v1_app_id(const QString &app_id)
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

void ForeignToplevelHandle::ztreeland_foreign_toplevel_handle_v1_identifier(const QString &identifier)
{
    qDebug() << Q_FUNC_INFO << identifier;
}

ShortcutManager::ShortcutManager()
    : QWaylandClientExtensionTemplate<ShortcutManager>(1)
{

}

ShortcutContext::ShortcutContext(struct ::ztreeland_shortcut_context_v1 *object)
    : QWaylandClientExtensionTemplate<ShortcutContext>(1)
    , QtWayland::ztreeland_shortcut_context_v1(object)
{

}

void ShortcutContext::ztreeland_shortcut_context_v1_shortcut(uint32_t keycode, uint32_t modify)
{
    qDebug() << Q_FUNC_INFO << keycode << modify;
    emit shortcutHappended(keycode, modify);
}

FakeSession::FakeSession(int argc, char* argv[])
    : QGuiApplication(argc, argv)
    , m_shortcutManager(new ShortcutManager)
    , m_toplevelManager(new ForeignToplevelManager)
    , m_extForeignToplevelList(new ExtForeignToplevelList)
{
    connect(m_shortcutManager, &ShortcutManager::activeChanged, this, [=] {
        qDebug() << m_shortcutManager->isActive();
        if (m_shortcutManager->isActive()) {

            ShortcutContext* context = new ShortcutContext(m_shortcutManager->get_shortcut_context());
            connect(context, &ShortcutContext::shortcutHappended, this, [=](uint32_t keycode, uint32_t modify) {
                auto keyEnum = static_cast<Qt::Key>(keycode);
                auto modifyEnum = static_cast<Qt::KeyboardModifiers>(modify);
                qDebug() << keyEnum << modifyEnum;
                if (keyEnum == Qt::Key_Super_L && modifyEnum == Qt::NoModifier) {
                    static QProcess process;
                    if (process.state() == QProcess::ProcessState::Running) {
                        process.kill();
                    }
                    process.start("dofi", {"-S", "run"});
                    return;
                }
                if (keyEnum == Qt::Key_T && modifyEnum.testFlags(Qt::ControlModifier | Qt::AltModifier)) {
                    QProcess::startDetached("x-terminal-emulator");
                    return;
                }
            });
        }
    });

    connect(m_toplevelManager, &ForeignToplevelManager::newForeignToplevelHandle, this, [=](ForeignToplevelHandle *handle) {
        connect(handle, &ForeignToplevelHandle::pidChanged, this, [=](uint32_t pid) {
            qDebug() << "toplevel pid: " << pid;
        });
    });

    connect(m_extForeignToplevelList, &ExtForeignToplevelList::newToplevel, this, [=](ExtForeignToplevelHandle *handle) {
        connect(handle, &ExtForeignToplevelHandle::appIdChanged, this, [=](const QString &appId) {
            qDebug() << "toplevel appid: " << appId;
        });
    });

    emit m_shortcutManager->activeChanged();
}

int main (int argc, char *argv[]) {
    FakeSession helper(argc, argv);

    return helper.exec();
}