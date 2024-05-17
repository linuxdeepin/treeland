// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcutmanager.h"

#include "qshortcutmanager.h"

#include <wayland-server-core.h>
#include <wayland-util.h>

#include <qwdisplay.h>

#include <QAction>
#include <QDebug>
#include <QTimer>

#include <pwd.h>
#include <sys/socket.h>
#include <unistd.h>

class ShortcutManagerV1Private : public WObjectPrivate
{
public:
    ShortcutManagerV1Private(ShortcutManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    ~ShortcutManagerV1Private() = default;

    W_DECLARE_PUBLIC(ShortcutManagerV1)

    QTreeLandShortcutManagerV1 *manager = nullptr;
    Helper *helper = nullptr;
};

ShortcutManagerV1::ShortcutManagerV1(QObject *parent)
    : Waylib::Server::WQuickWaylandServerInterface(parent)
    , WObject(*new ShortcutManagerV1Private(this), nullptr)
{
}

void ShortcutManagerV1::setHelper(Helper *helper)
{
    W_D(ShortcutManagerV1);

    d->helper = helper;
}

Helper *ShortcutManagerV1::helper()
{
    W_D(ShortcutManagerV1);

    return d->helper;
}

void ShortcutManagerV1::create()
{
    W_D(ShortcutManagerV1);

    d->manager = QTreeLandShortcutManagerV1::create(server()->handle());
    connect(d->manager,
            &QTreeLandShortcutManagerV1::newContext,
            this,
            [this, d](QTreeLandShortcutContextV1 *context) {
                QAction *action = new QAction(context);
                action->setShortcut(QString(context->handle()->key));

                struct wl_client *client =
                    wl_resource_get_client(context->handle()->manager->client);
                uid_t uid;
                wl_client_get_credentials(client, nullptr, &uid, nullptr);
                struct passwd *pw = getpwuid(uid);
                const QString username(pw->pw_name);

                connect(action, &QAction::triggered, this, [context] {
                    context->happend();
                });

                connect(context,
                        &QTreeLandShortcutContextV1::beforeDestroy,
                        this,
                        [d, username, action] {
                            d->helper->removeAction(username, action);
                        });

                d->helper->addAction(username, action);
            });
}
