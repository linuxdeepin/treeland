// Copyright (C) 2025 misaka18931 <miruku2937@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsessionlockmanager.h"
#include "private/wglobal_p.h"
#include "wsessionlock.h"

#include <wlr/types/wlr_session_lock_v1.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSessionLockManagerPrivate : public WWrapObjectPrivate
{
public:
    WSessionLockManagerPrivate(WSessionLockManager *qq);
    // begin slot function
    void onNewLock(wlr_session_lock_v1 *lock);
    void onLockDestroy(wlr_session_lock_v1 *lock);
    // end slot function

    W_DECLARE_PUBLIC(WSessionLockManager)

    WScopedListener m_newLockListener;
    QVector<WSessionLock*> lockList;
};

WSessionLockManagerPrivate::WSessionLockManagerPrivate(WSessionLockManager *qq)
    : WWrapObjectPrivate(qq)
{
    
}

void WSessionLockManagerPrivate::onNewLock(wlr_session_lock_v1 *sessionLock)
{
    W_Q(WSessionLockManager);

    auto server = q->server();
    WSessionLock *lock = new WSessionLock(sessionLock, server);
    lock->setParent(server);
    Q_ASSERT(lock->parent() == server);
    
    lock->safeConnect(&WSessionLock::aboutToBeInvalidated, q, [this, sessionLock]() {
        onLockDestroy(sessionLock);
    });

    lockList.append(lock);
    Q_EMIT q->lockCreated(lock);
}

void WSessionLockManagerPrivate::onLockDestroy(wlr_session_lock_v1 *sessionLock)
{
    W_Q(WSessionLockManager);
    WSessionLock *lock = WSessionLock::fromHandle(sessionLock);
    
    bool ok = lockList.removeOne(lock);
    Q_ASSERT(ok);
    Q_EMIT q->lockDestroyed(lock);
    lock->safeDeleteLater();
}

WSessionLockManager::WSessionLockManager(QObject *parent) :
    WWrapObject(*new WSessionLockManagerPrivate(this), parent)
{

}

QByteArrayView WSessionLockManager::interfaceName() const
{
    return "ext_session_lock_manager_v1";
}

void WSessionLockManager::create(WServer *server)
{
    W_D(WSessionLockManager);

    m_handle = wlr_session_lock_manager_v1_create(server->handle());
    auto *session_lock_manager = static_cast<wlr_session_lock_manager_v1*>(m_handle);
    d->m_newLockListener.connect(&session_lock_manager->events.new_lock, [d](wl_listener *, void *data) {
        d->onNewLock(static_cast<wlr_session_lock_v1*>(data));
    });
}

QVector<WSessionLock*> WSessionLockManager::lockList() const
{
    W_DC(WSessionLockManager);
    return d->lockList;
}

void WSessionLockManager::destroy([[maybe_unused]] WServer *server)
{
    W_D(WSessionLockManager);
    d->m_newLockListener.remove();

    auto lockList = d->lockList;
    d->lockList.clear();
    for (auto lock : std::as_const(lockList)) {
        Q_EMIT lockDestroyed(lock);
        lock->safeDeleteLater();
    }
}

wl_global *WSessionLockManager::global() const
{
    return static_cast<wlr_session_lock_manager_v1*>(m_handle)->global;
}

WAYLIB_SERVER_END_NAMESPACE
