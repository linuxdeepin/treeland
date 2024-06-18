// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcutmanager.h"

#include "impl/shortcut_manager_impl.h"

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <qwdisplay.h>

#include <QAction>

#include <pwd.h>
#include <sys/socket.h>
#include <unistd.h>

ShortcutManagerV1::ShortcutManagerV1(QObject *parent)
    : Waylib::Server::WQuickWaylandServerInterface(parent)
{
}

void ShortcutManagerV1::setHelper(Helper *helper)
{
    m_helper = helper;
}

Helper *ShortcutManagerV1::helper()
{
    return m_helper;
}

WServerInterface *ShortcutManagerV1::create()
{
    m_manager = treeland_shortcut_manager_v1::create(server()->handle());
    connect(m_manager,
            &treeland_shortcut_manager_v1::newContext,
            this,
            &ShortcutManagerV1::onNewContext);
    return new WServerInterface(m_manager, m_manager->global);
}

void ShortcutManagerV1::onNewContext(uid_t uid, treeland_shortcut_context_v1 *context)
{
    QAction *action = new QAction(context);
    action->setShortcut(QString(context->key));

    passwd *pw = getpwuid(uid);
    const QString username(pw->pw_name);

    connect(action, &QAction::triggered, this, [context] {
        context->send_shortcut();
    });

    connect(context, &treeland_shortcut_context_v1::beforeDestroy, this, [this, username, action] {
        m_helper->removeAction(username, action);
    });

    m_helper->addAction(username, action);
}
