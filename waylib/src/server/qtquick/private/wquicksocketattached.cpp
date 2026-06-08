// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wquicksocketattached_p.h"
#include "wsocket.h"

#include <QQmlInfo>

WAYLIB_SERVER_BEGIN_NAMESPACE

WQuickSocketAttached::WQuickSocketAttached(WSocket *socket)
    : QObject(socket)
{

}

WSocket *WQuickSocketAttached::socket() const
{
    return qobject_cast<WSocket*>(parent());
}

WSocket *WQuickSocketAttached::rootSocket() const
{
    return socket()->rootSocket();
}

WQuickSocketAttached *WQuickSocketAttached::qmlAttachedProperties(QObject *target)
{
    if (auto wobject = dynamic_cast<WObject*>(target)) {
        auto client = wobject->waylandClient();
        if (client) {
            auto socket = client->socket();
            Q_ASSERT(socket);
            auto attached = socket->findChild<WQuickSocketAttached*>(QString(), Qt::FindDirectChildrenOnly);

            if (!attached)
                attached = new WQuickSocketAttached(socket);

            return attached;
        }
    }

    return nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
