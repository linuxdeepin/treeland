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

ShortcutV1::ShortcutV1(QObject *parent)
    : QObject(parent)
{
}

void ShortcutV1::onNewContext(uid_t uid, treeland_shortcut_context_v1 *context)
{
    QAction *action = new QAction(context);
    action->setShortcut(QString(context->key));

    connect(action, &QAction::triggered, this, [context] {
        context->send_shortcut();
    });

    connect(context, &treeland_shortcut_context_v1::before_destroy, this, [this, uid, action] {
        m_actions.remove(uid);
        action->deleteLater();
    });

    if (!m_actions.count(uid)) {
        m_actions[uid] = {};
    }

    auto find = std::ranges::find_if(m_actions[uid], [action](QAction *a) {
        return a->shortcut() == action->shortcut();
    });

    if (find == m_actions[uid].end()) {
        m_actions[uid].push_back(action);
    }
}

std::vector<QAction *> ShortcutV1::actions(uid_t uid) const
{
    return m_actions[uid];
}

void ShortcutV1::triggerMetaKey(uid_t uid)
{
    for (auto *action : m_actions[uid]) {
        if (action->shortcut().toString() == "Meta") {
            action->trigger();
            break;
        }
    }
}

void ShortcutV1::create(WServer *server)
{
    m_manager = treeland_shortcut_manager_v1::create(server->handle());
    connect(m_manager, &treeland_shortcut_manager_v1::newContext, this, &ShortcutV1::onNewContext);
}

void ShortcutV1::destroy(WServer *server) { }

wl_global *ShortcutV1::global() const
{
    return m_manager->global;
}

QByteArrayView ShortcutV1::interfaceName() const
{
    return "treeland_shortcut_manager_v1";
}
