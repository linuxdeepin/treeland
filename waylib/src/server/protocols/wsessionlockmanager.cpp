// Copyright (C) 2025 misaka18931 <miruku2937@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsessionlockmanager.h"
#include "private/wglobal_p.h"
#include "wsessionlock.h"

#include <utility>

extern "C" {
#include <wlr/types/wlr_session_lock_v1.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSessionLockManagerPrivate : public WObjectPrivate
{
public:
    WSessionLockManagerPrivate(WSessionLockManager *qq)
        : WObjectPrivate(qq)
    {
    }

    void onNewLock(wlr_session_lock_v1 *nativeLock)
    {
        W_Q(WSessionLockManager);
        auto *lock = new WSessionLock(nativeLock, q->server());
        Q_ASSERT(lock->parent() == q->server());
        lockList.append(lock);
        QObject::connect(lock, &WWrapObject::aboutToBeInvalidated, q, [this, lock] {
            if (!lockList.removeOne(lock))
                return;
            Q_EMIT q_func()->lockDestroyed(lock);
        });
        Q_EMIT q->lockCreated(lock);
    }

    void releaseLocks()
    {
        const auto locks = std::exchange(lockList, {});
        W_Q(WSessionLockManager);
        for (auto *lock : locks) {
            Q_EMIT q->lockDestroyed(lock);
            lock->safeDeleteLater();
        }
    }

    W_DECLARE_PUBLIC(WSessionLockManager)

    WNativeListener newLockListener;
    QVector<WSessionLock *> lockList;
};

WSessionLockManager::WSessionLockManager(QObject *parent)
    : QObject(parent)
    , WObject(*new WSessionLockManagerPrivate(this))
{
}

wlr_session_lock_manager_v1 *WSessionLockManager::handle() const
{
    return nativeInterface<wlr_session_lock_manager_v1>();
}

QByteArrayView WSessionLockManager::interfaceName() const
{
    return "ext_session_lock_manager_v1";
}

void WSessionLockManager::create(WServer *server)
{
    W_D(WSessionLockManager);
    auto *manager = wlr_session_lock_manager_v1_create(server->handle());
    Q_ASSERT(manager);
    m_handle = manager;
    d->newLockListener.connect(&manager->events.new_lock, [d](void *data) {
        d->onNewLock(static_cast<wlr_session_lock_v1 *>(data));
    });
}

QVector<WSessionLock *> WSessionLockManager::lockList() const
{
    W_DC(WSessionLockManager);
    return d->lockList;
}

void WSessionLockManager::destroy([[maybe_unused]] WServer *server)
{
    W_D(WSessionLockManager);
    d->newLockListener.disconnect();
    d->releaseLocks();
    m_handle = nullptr;
}

wl_global *WSessionLockManager::global() const
{
    return handle() ? handle()->global : nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
