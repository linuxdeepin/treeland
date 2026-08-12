// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsessionlock.h"

#include "wsessionlocksurface.h"
#include "wscoplistener.h"
#include "private/wglobal_p.h"

#include <wlr_all.h>

#include <QPointer>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WSessionLockPrivate : public WObjectPrivate
{
public:
    WSessionLockPrivate(WSessionLock *qq, wlr_session_lock_v1 *handle)
        : WObjectPrivate(qq)
        , m_handle(handle)
    {
        Q_ASSERT(handle);
        handle->data = qq;
    }
    void setStatus(WSessionLock::LockState status) {
        m_status = status;
    }
    // begin slot function
    void onNewSurface(wlr_session_lock_surface_v1 *surface);
    void onSurfaceDestroy(WSessionLockSurface *lockSurface);
    // end slot function

    void lock();
    void finish();

    inline wlr_session_lock_v1 *handle() const {
        return m_handle;
    }

    W_DECLARE_PUBLIC(WSessionLock)

    QVector<WSessionLockSurface*> surfaceList;
    WSessionLock::LockState m_status{WSessionLock::LockState::Created};

private:
    // The session-lock owner destroys this handle after notifying the
    // wrapper. Keep the address stable through that callback.
    wlr_session_lock_v1 *m_handle = nullptr;
};

void WSessionLockPrivate::onNewSurface(wlr_session_lock_surface_v1 *surface)
{
    W_Q(WSessionLock);
    WSessionLockSurface *lockSurface = new WSessionLockSurface(surface);
    // The lock created the surface, so it releases it (owner rule).

    // Register the destroy listener on the surface wrapper (owner = this
    // lock): released automatically by ~WObject when the wrapper is deleted.
    lockSurface->listeners(q_ptr)->add(&surface->events.destroy, this,
        [this, lockSurface] (void *) {
        // Detach our own listener group: wlr_session_lock_surface destroy
        // asserts the listener lists are empty after emitting. Safe from
        // inside a callback of this list (the closure outlives the emission).
        lockSurface->removeListeners(q_ptr);
        onSurfaceDestroy(lockSurface);
    });

    surfaceList.append(lockSurface);
    Q_EMIT q->surfaceAdded(lockSurface);
}

void WSessionLockPrivate::onSurfaceDestroy(WSessionLockSurface *lockSurface)
{
    W_Q(WSessionLock);
    // invalidate() clears handle->data before invalidated() fires, so the
    // reverse fromHandle() lookup would return null here; use the captured
    // pointer instead.
    bool ok = surfaceList.removeOne(lockSurface);
    if (!ok) {
        // surface may be removed by session lock
        return;
    }
    Q_EMIT q->surfaceRemoved(lockSurface);
    delete lockSurface;
}

void WSessionLockPrivate::lock()
{
    W_Q(WSessionLock);
    Q_ASSERT(m_status == WSessionLock::LockState::Created);
    wlr_session_lock_v1_send_locked(m_handle);
    m_status = WSessionLock::LockState::Locked;
    Q_EMIT q->locked();
}

void WSessionLockPrivate::finish()
{
    W_Q(WSessionLock);
    Q_ASSERT(m_status == WSessionLock::LockState::Created);
    // wlr_session_lock_v1_destroy() synchronously emits the native destroy
    // signal, from which the manager deletes this wrapper. Complete the
    // state transition and notify observers BEFORE destroying: the destroy
    // callback then observes Finished and emits nothing (no spurious
    // canceled()).
    m_status = WSessionLock::LockState::Finished;
    // finished() is a synchronous Qt signal: a receiver may delete this
    // wrapper inside the slot. Save the native handle and guard the wrapper
    // before emitting so the destroy below never touches freed members.
    auto *handle = m_handle;
    QPointer<WSessionLock> guard(q);
    Q_EMIT q->finished();
    if (!guard)
        return; // wrapper deleted from the signal; the native lock is
                // released with the client/display, no UAF on this side
    wlr_session_lock_v1_destroy(handle);
}

WSessionLock::WSessionLock(wlr_session_lock_v1 *handle)
    : QObject(nullptr)
    , WObject(*new WSessionLockPrivate(this, handle))
{
    W_D(WSessionLock);
    // connect new_surface and unlock signals
    listeners()->add(&handle->events.new_surface, d,
        &WSessionLockPrivate::onNewSurface);
    listeners()->add(&handle->events.unlock, this, [d, this] (void *) {
        Q_ASSERT(d->m_status == LockState::Locked);
        Q_EMIT unlocked();
    });
    // State transition before the base class teardown listener (registered
    // after this one) invalidates and deletes the wrapper.
    listeners()->add(&handle->events.destroy, this, [d, this] (void *) {
        switch (lockState()) {
            case LockState::Created:
                d->setStatus(LockState::Canceled);
                Q_EMIT canceled();
                break;
            case LockState::Locked:
                d->setStatus(LockState::Abandoned);
                Q_EMIT abandoned();
                break;
            case LockState::Finished:
            case LockState::Unlocked:
                break;
            default:
                break;
        }
    });
    d->m_handle = handle;
}

WSessionLock::~WSessionLock()
{
    teardown();
    W_D(WSessionLock);
    // Clear the reverse fromHandle() mapping. The manager destroys this
    // wrapper from the lock's destroy callback (or while the native handle
    // is still alive), so the handle is valid here.
    if (d->m_handle && d->m_handle->data == this)
        d->m_handle->data = nullptr;
    // Owner rule: this object created the WSessionLockSurface wrappers,
    // release any that are still alive (each surface's listeners are
    // released by its own ~WObject).
    QVector<WSessionLockSurface*> list;
    list.swap(d->surfaceList);
    for (auto *surface : std::as_const(list)) {
        Q_EMIT surfaceRemoved(surface);
        delete surface;
    }
}

WSessionLock *WSessionLock::fromHandle(wlr_session_lock_v1 *handle)
{
    if (!handle)
        return nullptr;
    return static_cast<WSessionLock*>(handle->data);
}

wlr_session_lock_v1 *WSessionLock::handle() const
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
