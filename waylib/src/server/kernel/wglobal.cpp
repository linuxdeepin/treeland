// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wglobal.h"
#include "wsocket.h"
#include "private/wglobal_p.h"

#include <private/qobject_p_p.h>
#include <QCursor>
#include <QLoggingCategory>

WAYLIB_SERVER_BEGIN_NAMESPACE

WClient *WObject::waylandClient() const
{
    auto client = w_d_ptr->waylandClient();
    if (!client)
        return nullptr;

    auto wclient = WClient::get(client);
    Q_ASSERT(wclient);

    return wclient;
}

pid_t WObject::pid() const
{
    auto client = waylandClient();
    if (!client)
        return 0;
    auto credentials = client->credentials();
    if (!credentials)
        return 0;
    return credentials->pid;
}

int WObject::pidFD() const
{
    auto client = waylandClient();
    if (!client)
        return -1;
    return client->pidFD();
}

WObject::WObject(WObjectPrivate &dd, WObject *)
    : w_d_ptr(&dd)
{

}

int WObject::indexOfAttachedData(const void *owner) const
{
    W_DC(WObject);
    return d->indexOfAttachedData(owner);
}

const QList<std::pair<const void *, void *>> &WObject::attachedDatas() const
{
    W_DC(WObject);
    return d->attachedDatas;
}

QList<std::pair<const void *, void *>> &WObject::attachedDatas()
{
    W_D(WObject);
    return d->attachedDatas;
}

WObject::~WObject()
{
    w_d_ptr->invalidate();
}

WObjectPrivate *WObjectPrivate::get(WObject *qq)
{
    return qq->d_func();
}

WObjectPrivate::WObjectPrivate(WObject *qq)
    : q_ptr(qq)
{

}
WObjectPrivate::~WObjectPrivate()
{
}

void WObjectPrivate::invalidate(QObject *object)
{
    if (invalidated)
        return;
    invalidated = true;

    if (object) {
        QMetaObject::invokeMethod(object, "aboutToBeInvalidated", Qt::DirectConnection);

        auto d = QObjectPrivate::get(object);
        if (!d->isDeletingChildren && d->declarativeData && QAbstractDeclarativeData::destroyed) {
            QAbstractDeclarativeData::destroyed(d->declarativeData, object);
            d->declarativeData = nullptr;
        }
    }

    instantRelease();

    if (object)
        QMetaObject::invokeMethod(object, "invalidated", Qt::DirectConnection);
}

bool WObject::isInvalidated() const
{
    return w_d_ptr->isInvalidated();
}

bool WObject::safeDisconnect(const QObject *receiver)
{
    auto *object = dynamic_cast<QObject *>(this);
    Q_ASSERT(object);
    return QObject::disconnect(object, nullptr, receiver, nullptr);
}

bool WObject::safeDisconnect(const QMetaObject::Connection &connection)
{
    return QObject::disconnect(connection);
}

void WObject::safeDeleteLater()
{
    auto *object = dynamic_cast<QObject *>(this);
    Q_ASSERT(object);
    w_d_ptr->invalidate(object);
    object->deleteLater();
}

void WObject::invalidate()
{
    auto *object = dynamic_cast<QObject *>(this);
    w_d_ptr->invalidate(object);
}

bool WGlobal::isInvalidCursor(const QCursor &c)
{
    return static_cast<CursorShape>(c.shape()) == CursorShape::Invalid;
}

bool WGlobal::isClientResourceCursor(const QCursor &c)
{
    return static_cast<CursorShape>(c.shape()) == CursorShape::ClientResource;
}

WAYLIB_SERVER_END_NAMESPACE
