// Copyright (C) 2023 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcutmanager.h"

#include <qwdisplay.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-util.h>

#include <QDebug>
#include <QTimer>
#include <QAction>

#include "qshortcutmanager.h"

class ShortcutManagerV1Private : public WObjectPrivate {
public:
    ShortcutManagerV1Private(ShortcutManagerV1 *qq)
        : WObjectPrivate(qq) {}
    ~ShortcutManagerV1Private() = default;

    W_DECLARE_PUBLIC(ShortcutManagerV1)

    QTreeLandShortcutManagerV1 *manager = nullptr;
    TreeLandHelper *helper = nullptr;
};


ShortcutManagerV1::ShortcutManagerV1(QObject *parent)
    : Waylib::Server::WQuickWaylandServerInterface(parent)
    , WObject(*new ShortcutManagerV1Private(this), nullptr)
{
}

void ShortcutManagerV1::setHelper(TreeLandHelper *helper) {
    W_D(ShortcutManagerV1);

    d->helper = helper;
}

void ShortcutManagerV1::create()
{
    W_D(ShortcutManagerV1);

    d->manager = QTreeLandShortcutManagerV1::create(server()->handle());
    connect(d->manager, &QTreeLandShortcutManagerV1::newContext, this, [this, d](QTreeLandShortcutContextV1 *context) {
        QAction *action = new QAction(context);
        action->setShortcut(QString(context->handle()->key));
        connect(action, &QAction::triggered, this, [context] {
            context->happend();
        });
        connect(context, &QTreeLandShortcutContextV1::beforeDestroy, this, [d, action] {
            d->helper->removeAction(action);
        });

        d->helper->addAction(action);
    });
}
