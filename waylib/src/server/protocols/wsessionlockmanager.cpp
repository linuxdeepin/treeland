// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsessionlockmanager.h"
#include "private/wglobal_p.h"
#include "wscoplistener.h"
#include "wsessionlock.h"

#include <wlr_all.h>
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSessionLockManagerPrivate : public WObjectPrivate
{
public:
    WSessionLockManagerPrivate(WSessionLockManager *qq);
    // begin slot function
    void onNewLock(wlr_session_lock_v1 *lock);
    void onLockDestroy(WSessionLock *lock);
    // end slot function

    W_DECLARE_PUBLIC(WSessionLockManager)

    QVector<WSessionLock*> lockList;
};

WSessionLockManagerPrivate::WSessionLockManagerPrivate(WSessionLockManager *qq)
    : WObjectPrivate(qq)
{

}

void WSessionLockManagerPrivate::onNewLock(wlr_session_lock_v1 *sessionLock)
{
    W_Q(WSessionLockManager);
    auto lock = new WSessionLock(sessionLock);
    // Register the destroy listener on the lock wrapper (owner = this
    // manager): released automatically by ~WObject when the wrapper is
    // deleted.
    lock->listeners(q_ptr)->add(&sessionLock->events.destroy, this,
        [this, lock] (void *) {
        // Detach our own listener group: wlr_session_lock_v1 destroy
        // asserts the listener lists are empty after emitting. Safe from
        // inside a callback of this list (the closure outlives the emission).
        lock->removeListeners(q_ptr);
        onLockDestroy(lock);
    });
    lockList.append(lock);
    Q_EMIT q->lockCreated(lock);
}

void WSessionLockManagerPrivate::onLockDestroy(WSessionLock *lock)
{
    W_Q(WSessionLockManager);
    bool ok = lockList.removeOne(lock);
    Q_ASSERT(ok);
    Q_EMIT q->lockDestroyed(lock);
    delete lock;
}

WSessionLockManager::WSessionLockManager() :
    QObject(nullptr),
    WObject(*new WSessionLockManagerPrivate(this))
{
}

QByteArrayView WSessionLockManager::interfaceName() const
{
    return "ext_session_lock_manager_v1";
}

wlr_session_lock_manager_v1 *WSessionLockManager::handle() const
{
    return reinterpret_cast<wlr_session_lock_manager_v1*>(m_handle);
}

void WSessionLockManager::create(WServer *server)
{
    W_D(WSessionLockManager);

    auto *session_lock_manager = wlr_session_lock_manager_v1_create(server->handle());
    Q_ASSERT(session_lock_manager);
    listeners()->add(&session_lock_manager->events.new_lock, d,
        &WSessionLockManagerPrivate::onNewLock);
    m_handle = session_lock_manager;
}

QVector<WSessionLock*> WSessionLockManager::lockList() const
{
    W_DC(WSessionLockManager);
    return d->lockList;
}

void WSessionLockManager::destroy([[maybe_unused]] WServer *server)
{
    W_D(WSessionLockManager);

    auto lockList = d->lockList;
    d->lockList.clear();
    for (auto lock : std::as_const(lockList)) {
        Q_EMIT lockDestroyed(lock);
        delete lock;
    }
    // Clear the dangling handle now: the wlr_session_lock_manager_v1 is
    // reclaimed by display.reset() in WServer::stop(), but nulling m_handle
    // immediately makes handle()/global() return null instead of dangling.
    m_handle = nullptr;
}

wl_global *WSessionLockManager::global() const
{
    if (!m_handle)
        return nullptr;
    auto handle = reinterpret_cast<wlr_session_lock_manager_v1*>(m_handle);
    return handle->global;
}

WAYLIB_SERVER_END_NAMESPACE
