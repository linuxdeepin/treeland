// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wglobal.h"
#include "wsocket.h"
#include "private/wglobal_p.h"

#include <private/qobject_p_p.h>
#include <QCursor>
#include <QLoggingCategory>

#ifdef QT_DEBUG
Q_LOGGING_CATEGORY(lcGeneral, "waylib.general", QtDebugMsg);
#else
Q_LOGGING_CATEGORY(lcGeneral, "waylib.general", QtInfoMsg);
#endif

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

WWrapObject::WWrapObject(QObject *parent)
    : WWrapObject(*new WWrapObjectPrivate(this), parent)
{

}

WWrapObject::WWrapObject(WWrapObjectPrivate &d, QObject *parent)
    : WObject(d, nullptr)
    , QObject(parent)
{

}

WWrapObject::~WWrapObject()
{
    W_D(WWrapObject);
    d->invalidate();
}

WWrapObjectPrivate::WWrapObjectPrivate(WWrapObject *q)
    : WObjectPrivate(q)
    , invalidated(false)
{

}

WWrapObjectPrivate::~WWrapObjectPrivate()
{
    // Must call invalidate before destroy WWrapObject
    Q_ASSERT(invalidated);
}

void WWrapObjectPrivate::initHandle(QW_NAMESPACE::qw_object_basic *handle)
{
    Q_ASSERT(!m_handle);
    Q_ASSERT(!invalidated);
    m_handle = handle;
}

static inline QObjectPrivate::Connection *getConnectionDPtr(const QMetaObject::Connection *connection)
{
    static_assert(sizeof(connection) == sizeof(void*),
                  "Please check how to use QMetaObject::Connection::d_ptr");
    return *reinterpret_cast<QObjectPrivate::Connection**>(const_cast<QMetaObject::Connection*>(connection));
}

void WWrapObjectPrivate::invalidate()
{
    if (invalidated)
        return;
    invalidated = true;

    W_Q(WWrapObject);

    Q_EMIT q->aboutToBeInvalidated();

    auto d = QObjectPrivate::get(q);
    if (!d->isDeletingChildren && d->declarativeData && QAbstractDeclarativeData::destroyed) {
        QAbstractDeclarativeData::destroyed(d->declarativeData, q);
        d->declarativeData = nullptr;
    }

    instantRelease();
    for (const auto &connection : std::as_const(connectionsWithHandle)) {
        QObject::disconnect(connection);
    }
    if (m_handle) {
        m_handle->disconnect(q);
        m_handle = nullptr;
    }
    connectionsWithHandle.clear();

    Q_EMIT q->invalidated();
}

bool WWrapObject::safeDisconnect(const QObject *receiver)
{
    W_D(WWrapObject);

    bool ok = false;
    for (int i = 0; i < d->connectionsWithHandle.size(); ++i) {
        const QMetaObject::Connection &connection = d->connectionsWithHandle.at(i);
        auto c_d = getConnectionDPtr(&connection);
        if (c_d->receiver == receiver) {
            if (QObject::disconnect(connection))
                ok = true;

            d->connectionsWithHandle.removeAt(i);
            // reset index
            --i;
        }
    }

    if (disconnect(receiver))
        ok = true;

    return ok;
}

bool WWrapObject::safeDisconnect(const QMetaObject::Connection &connection)
{
    W_D(WWrapObject);
    int index = d->connectionsWithHandle.indexOf(connection);
    if (index < 0) {
        auto c_d = getConnectionDPtr(&connection);
        if (c_d->sender != this)
            return false;
        return disconnect(connection);
    }
    d->connectionsWithHandle.removeAt(index);
    return QObject::disconnect(connection);
}

void WWrapObject::safeDeleteLater()
{
    W_D(WWrapObject);
    d->invalidate();
    deleteLater();
}

QW_NAMESPACE::qw_object_basic *WWrapObject::handle() const
{
    W_DC(WWrapObject);
    return d->m_handle;
}

bool WWrapObject::isInvalidated() const
{
    W_DC(WWrapObject);
    return d->invalidated;
}

void WWrapObject::invalidate()
{
    W_D(WWrapObject);
    d->invalidate();
}

void WWrapObject::initHandle(QW_NAMESPACE::qw_object_basic *handle)
{
    W_D(WWrapObject);
    d->initHandle(handle);
}

void WWrapObject::beginSafeConnect()
{

}

void WWrapObject::endSafeConnect(const QMetaObject::Connection &connection)
{
    W_D(WWrapObject);
    if (connection)
        d->connectionsWithHandle.append(connection);
}

#ifdef QT_DEBUG
bool WWrapObject::event(QEvent *event)
{
    if (event->type() == QEvent::DeferredDelete) {
        Q_ASSERT(d_func()->invalidated);
    }

    return QObject::event(event);
}
#endif

bool WGlobal::isInvalidCursor(const QCursor &c)
{
    return static_cast<CursorShape>(c.shape()) == CursorShape::Invalid;
}

bool WGlobal::isClientResourceCursor(const QCursor &c)
{
    return static_cast<CursorShape>(c.shape()) == CursorShape::ClientResource;
}

WAYLIB_SERVER_END_NAMESPACE
