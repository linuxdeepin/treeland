// Copyright (C) 2025 misaka18931 <miruku2937@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsessionlockmanager.h"
#include "private/wglobal_p.h"
#include "wsessionlock.h"

#include <qwsessionlockv1.h>
#include <qwdisplay.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

using QW_NAMESPACE::qw_session_lock_manager_v1;

class Q_DECL_HIDDEN WSessionLockManagerPrivate : public WWrapObjectPrivate
{
public:
    WSessionLockManagerPrivate(WSessionLockManager *qq);
    // begin slot function
    void onNewLock(qw_session_lock_v1 *lock);
    void onLockDestroy(qw_session_lock_v1 *lock);
    // end slot function

    W_DECLARE_PUBLIC(WSessionLockManager)

    QVector<WSessionLock*> lockList;
};

WSessionLockManagerPrivate::WSessionLockManagerPrivate(WSessionLockManager *qq)
    : WWrapObjectPrivate(qq)
{
    
}

void WSessionLockManagerPrivate::onNewLock(qw_session_lock_v1 *sessionLock)
{
    W_Q(WSessionLockManager);

    auto server = q->server();
    WSessionLock *lock = new WSessionLock(sessionLock, server);
    lock->setParent(server);
    Q_ASSERT(lock->parent() == server);
    
    lock->safeConnect(&qw_session_lock_v1::before_destroy, q, [this, sessionLock]() {
        onLockDestroy(sessionLock);
    });

    lockList.append(lock);
    Q_EMIT q->lockCreated(lock);
}

void WSessionLockManagerPrivate::onLockDestroy(qw_session_lock_v1 *sessionLock)
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

    auto *session_lock_manager = qw_session_lock_manager_v1::create(*server->handle());
    connect(session_lock_manager, &qw_session_lock_manager_v1::notify_new_lock, this, [d](wlr_session_lock_v1 *lock) {
        d->onNewLock(qw_session_lock_v1::from(lock));
    });
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
        lock->safeDeleteLater();
    }
}

wl_global *WSessionLockManager::global() const
{
    auto handle = nativeInterface<qw_session_lock_manager_v1>();
    return handle->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
