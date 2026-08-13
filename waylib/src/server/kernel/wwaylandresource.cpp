// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wwaylandresource.h"
#include "private/wwaylandresource_p.h"
#include "wsocket.h"
#include "wayliblogging.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

WWaylandResource::WWaylandResource(WWaylandResourcePrivate &dd, WObject *parent)
    : WObject(dd, parent)
{
}

WClient *WWaylandResource::waylandClient() const
{
    W_DC(WWaylandResource);
    auto client = d->waylandClient();
    if (!client)
        return nullptr;

    auto wclient = WClient::get(client);
    Q_ASSERT(wclient);

    return wclient;
}

pid_t WWaylandResource::pid() const
{
    auto client = waylandClient();
    if (!client)
        return 0;
    auto credentials = client->credentials();
    if (!credentials)
        return 0;
    return credentials->pid;
}

int WWaylandResource::pidFD() const
{
    auto client = waylandClient();
    if (!client)
        return -1;
    return client->pidFD();
}

WAYLIB_SERVER_END_NAMESPACE
