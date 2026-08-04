// Copyright (C) 2025-2026 misaka18931 <miruku2937@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wsessionlock.h"

#include "wsessionlocksurface.h"
#include "private/wglobal_p.h"

#include <utility>

extern "C" {
#include <wlr/types/wlr_session_lock_v1.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

using SessionLockRegistry = QHash<const wlr_session_lock_v1 *, WSessionLock *>;
Q_GLOBAL_STATIC(SessionLockRegistry, s_sessionLocks)

class Q_DECL_HIDDEN WSessionLockPrivate : public WWrapObjectPrivate
{
public:
    WSessionLockPrivate(WSessionLock *qq, wlr_session_lock_v1 *handle)
        : WWrapObjectPrivate(qq)
        , lockHandle(handle)
    {
        Q_ASSERT(lockHandle);
        Q_ASSERT(!s_sessionLocks->contains(lockHandle));
        s_sessionLocks->insert(lockHandle, qq);
    }

    inline wlr_session_lock_v1 *handle() const { return lockHandle; }

    void instantRelease() override;
    void onNewSurface(wlr_session_lock_surface_v1 *nativeSurface);
    void releaseSurfaces();
    void connectNativeEvents();
    void disconnectNativeEvents();
    void lock();
    void finish();

    W_DECLARE_PUBLIC(WSessionLock)

    wlr_session_lock_v1 *lockHandle = nullptr;
    WNativeListener newSurfaceListener;
    WNativeListener unlockListener;
    WNativeListener destroyListener;
    QVector<WSessionLockSurface *> surfaceList;
    WSessionLock::LockState status = WSessionLock::LockState::Created;
};

void WSessionLockPrivate::releaseSurfaces()
{
    const auto surfaces = std::exchange(surfaceList, {});
    W_Q(WSessionLock);
    for (auto *surface : surfaces) {
        Q_EMIT q->surfaceRemoved(surface);
        surface->safeDeleteLater();
    }
}

void WSessionLockPrivate::instantRelease()
{
    if (lockHandle) {
        disconnectNativeEvents();
        s_sessionLocks->remove(lockHandle);
        lockHandle = nullptr;
    }
    releaseSurfaces();
}

void WSessionLockPrivate::onNewSurface(wlr_session_lock_surface_v1 *nativeSurface)
{
    W_Q(WSessionLock);
    auto *surface = new WSessionLockSurface(nativeSurface, q);
    surfaceList.append(surface);
    QObject::connect(surface, &WWrapObject::aboutToBeInvalidated, q, [this, surface] {
        if (!surfaceList.removeOne(surface))
            return;
        Q_EMIT q_func()->surfaceRemoved(surface);
    });
    Q_EMIT q->surfaceAdded(surface);
}

void WSessionLockPrivate::connectNativeEvents()
{
    W_Q(WSessionLock);
    newSurfaceListener.connect(&handle()->events.new_surface, [this](void *data) {
        onNewSurface(static_cast<wlr_session_lock_surface_v1 *>(data));
    });
    unlockListener.connect(&handle()->events.unlock, [this, q](void *) {
        Q_ASSERT(status == WSessionLock::LockState::Locked);
        status = WSessionLock::LockState::Unlocked;
        Q_EMIT q->unlocked();
    });
    destroyListener.connect(&handle()->events.destroy, [this, q](void *) {
        switch (status) {
        case WSessionLock::LockState::Created:
            status = WSessionLock::LockState::Canceled;
            Q_EMIT q->canceled();
            break;
        case WSessionLock::LockState::Locked:
            status = WSessionLock::LockState::Abandoned;
            Q_EMIT q->abandoned();
            break;
        case WSessionLock::LockState::Finished:
        case WSessionLock::LockState::Unlocked:
            break;
        default:
            Q_UNREACHABLE();
        }
        q->safeDeleteLater();
    });
}

void WSessionLockPrivate::disconnectNativeEvents()
{
    newSurfaceListener.disconnect();
    unlockListener.disconnect();
    destroyListener.disconnect();
}

void WSessionLockPrivate::lock()
{
    W_Q(WSessionLock);
    Q_ASSERT(status == WSessionLock::LockState::Created);
    wlr_session_lock_v1_send_locked(handle());
    status = WSessionLock::LockState::Locked;
    Q_EMIT q->locked();
}

void WSessionLockPrivate::finish()
{
    W_Q(WSessionLock);
    Q_ASSERT(status == WSessionLock::LockState::Created);
    status = WSessionLock::LockState::Finished;
    wlr_session_lock_v1_destroy(handle());
    Q_EMIT q->finished();
}

WSessionLock::WSessionLock(wlr_session_lock_v1 *handle, QObject *parent)
    : WWrapObject(*new WSessionLockPrivate(this, handle), parent)
{
    d_func()->connectNativeEvents();
}

WSessionLock::~WSessionLock() = default;

WSessionLock *WSessionLock::fromHandle(wlr_session_lock_v1 *handle)
{
    return s_sessionLocks->value(handle);
}

wlr_session_lock_v1 *WSessionLock::handle() const
{
    W_DC(WSessionLock);
    return d->handle();
}

QVector<WSessionLockSurface *> WSessionLock::surfaceList() const
{
    W_DC(WSessionLock);
    return d->surfaceList;
}

WSessionLock::LockState WSessionLock::lockState() const
{
    W_DC(WSessionLock);
    return d->status;
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

void WSessionLock::finish()
{
    W_D(WSessionLock);
    d->finish();
}

WAYLIB_SERVER_END_NAMESPACE
