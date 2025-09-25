// Copyright (C) 2025 misaka18931 <miruku2937@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsessionlock.h"

#include "wsessionlocksurface.h"
#include "private/wglobal_p.h"

#include <qwsessionlockv1.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSessionLockPrivate : public WWrapObjectPrivate
{
public:
    WWRAP_HANDLE_FUNCTIONS(qw_session_lock_v1, wlr_session_lock_v1)

    WSessionLockPrivate(WSessionLock *qq, qw_session_lock_v1 *handle)
        : WWrapObjectPrivate(qq)
    {
        initHandle(handle);
    }
    void instantRelease() override;
    void setStatus(WSessionLock::LockState status) {
        m_status = status;
    }
    // begin slot function
    void onNewSurface(qw_session_lock_surface_v1 *surface);
    void onSurfaceDestroy(qw_session_lock_surface_v1 *surface);
    // end slot function
    
    void init();
    void lock();
    void finish();

    W_DECLARE_PUBLIC(WSessionLock)

    QVector<WSessionLockSurface*> surfaceList;
    WSessionLock::LockState m_status{WSessionLock::LockState::Created};
};

void WSessionLockPrivate::instantRelease()
{
    W_Q(WSessionLock);
    auto list = surfaceList;
    surfaceList.clear();
    for (auto surface : list) {
        q->surfaceRemoved(surface);
        surface->safeDeleteLater();
    }
}

void WSessionLockPrivate::onNewSurface(qw_session_lock_surface_v1 *surface)
{
    W_Q(WSessionLock);
    WSessionLockSurface *lockSurface = new WSessionLockSurface(surface, q);
    lockSurface->setParent(q);
    Q_ASSERT(lockSurface->parent() == q);
    
    lockSurface->safeConnect(&qw_session_lock_surface_v1::before_destroy, q, [this, surface] {
        onSurfaceDestroy(surface);
    });

    surfaceList.append(lockSurface);
    Q_EMIT q->surfaceAdded(lockSurface);
}

void WSessionLockPrivate::onSurfaceDestroy(qw_session_lock_surface_v1 *surface)
{
    W_Q(WSessionLock);
    WSessionLockSurface *lockSurface = WSessionLockSurface::fromHandle(surface);
    
    bool ok = surfaceList.removeOne(lockSurface);
    if (!ok) {
        // surface may be removed by session lock
        return;
    }
    Q_EMIT q->surfaceRemoved(lockSurface);
    lockSurface->safeDeleteLater();
}

void WSessionLockPrivate::init()
{
    W_Q(WSessionLock);
    handle()->set_data(this, q);
}

void WSessionLockPrivate::lock()
{
    W_Q(WSessionLock);
    Q_ASSERT(m_status == WSessionLock::LockState::Created);
    handle()->send_locked();
    m_status = WSessionLock::LockState::Locked;
    Q_EMIT q->locked();
}

void WSessionLockPrivate::finish()
{
    W_Q(WSessionLock);
    Q_ASSERT(m_status == WSessionLock::LockState::Created);
    handle()->destroy();
    m_status = WSessionLock::LockState::Finished;
    Q_EMIT q->finished();
}

WSessionLock::WSessionLock(qw_session_lock_v1 *handle, QObject *parent)
    : WWrapObject(*new WSessionLockPrivate(this, handle), parent)
{
    W_D(WSessionLock);
    d->init();
    // connect new_surface and unlock signals in
    connect(handle, &qw_session_lock_v1::notify_new_surface, this, [d](wlr_session_lock_surface_v1 *surface) {
        d->onNewSurface(qw_session_lock_surface_v1::from(surface));
    });
    connect(handle, &qw_session_lock_v1::notify_unlock, this, [d, this]() {
        Q_ASSERT(d->m_status == LockState::Locked);
        Q_EMIT unlocked();
    });
    connect(handle, &qw_session_lock_v1::before_destroy, this, [d, this]() {
        switch (lockState()) {
            case LockState::Created:
                d->setStatus(LockState::Canceled);
                Q_EMIT canceled();
                return;
            case LockState::Locked:
                d->setStatus(LockState::Abandoned);
                Q_EMIT abandoned();
                return;
            case LockState::Finished:
            case LockState::Unlocked:
                return;
            default:
                break;
        }
        Q_UNREACHABLE();
    });
}

WSessionLock::~WSessionLock()
{

}

WSessionLock *WSessionLock::fromHandle(qw_session_lock_v1 *handle)
{
    return handle->get_data<WSessionLock>();
}

qw_session_lock_v1 *WSessionLock::handle() const
{
    W_DC(WSessionLock);
    return d->handle();
}

QVector<WSessionLockSurface*> WSessionLock::surfaceList() const
{
    W_DC(WSessionLock);
    return d->surfaceList;
}

WSessionLock::LockState WSessionLock::lockState() const
{
    W_DC(WSessionLock);
    return d->m_status;
}

bool WSessionLock::isLocked() const
{
    W_DC(WSessionLock);
    return lockState() == LockState::Locked;
}

void WSessionLock::lock()
{
    W_D(WSessionLock);
    d->lock();
}

// Finish the lock (deny locking request)
void WSessionLock::finish()
{
    W_D(WSessionLock);
    d->finish();
}

WAYLIB_SERVER_END_NAMESPACE
