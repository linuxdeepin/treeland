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
    : QObject(parent)
    , WObject(d, nullptr)
{

}

WWrapObject::~WWrapObject()
{
    W_D(WWrapObject);
    d->invalidate();
}

WWrapObjectPrivate::WWrapObjectPrivate(WWrapObject *q)
    : WObjectPrivate(q)
    , m_nativeHandle(nullptr)
    , invalidated(false)
{

}

WWrapObjectPrivate::~WWrapObjectPrivate()
{
    // Must call invalidate before destroy WWrapObject
    Q_ASSERT(invalidated);
}

QHash<void*, WWrapObject*> &WWrapObjectPrivate::nativeHandleMap()
{
    static QHash<void*, WWrapObject*> map;
    return map;
}

WWrapObject *WWrapObjectPrivate::fromNativeHandle(const void *handle)
{
    if (!handle)
        return nullptr;
    return nativeHandleMap().value(const_cast<void*>(handle));
}

void WWrapObjectPrivate::initNativeHandle(void *handle, wl_signal *destroySignal)
{
    Q_ASSERT(!m_nativeHandle);
    Q_ASSERT(!invalidated);
    Q_ASSERT(handle);
    m_nativeHandle = handle;
    nativeHandleMap().insert(handle, q_func());
    if (destroySignal) {
        m_destroyListener.connect(destroySignal, [](wl_listener *listener, void *) {
            auto *self = WScopedListener::owner<WWrapObjectPrivate, &WWrapObjectPrivate::m_destroyListener>(listener);
            self->onNativeDestroy();
        });
    }
}

void WWrapObjectPrivate::onNativeDestroy()
{
    W_Q(WWrapObject);
    if (m_nativeHandle) {
        nativeHandleMap().remove(m_nativeHandle);
        m_nativeHandle = nullptr;
    }
    m_destroyListener.remove();
    q->safeDeleteLater();
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

    if (m_nativeHandle) {
        nativeHandleMap().remove(m_nativeHandle);
        m_nativeHandle = nullptr;
    }
    m_destroyListener.remove();

    Q_EMIT q->invalidated();
}

bool WWrapObject::safeDisconnect(const QObject *receiver)
{
    W_D(WWrapObject);
    return disconnect(receiver);
}

bool WWrapObject::safeDisconnect(const QMetaObject::Connection &connection)
{
    W_D(WWrapObject);
    auto c_d = getConnectionDPtr(&connection);
    if (c_d->sender != this)
        return false;
    return disconnect(connection);
}

void WWrapObject::safeDeleteLater()
{
    W_D(WWrapObject);
    d->invalidate();
    deleteLater();
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
