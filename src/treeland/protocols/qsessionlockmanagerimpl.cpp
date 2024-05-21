// Copyright (C) 2024 ssk-wh <fanpengcheng@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "qsessionlockmanagerimpl.h"

#include "shortcut_manager_impl.h"

#include <qwdisplay.h>
#include <qwglobal.h>
#include <qwsignalconnector.h>

#include <QHash>

QW_USE_NAMESPACE

class QSessionLockManagerImplPrivate : public QWObjectPrivate
{
public:
    QSessionLockManagerImplPrivate(ext_session_lock_manager_v1 *handle,
                                   bool isOwner,
                                   QSessionLockManagerImpl *qq)
        : QWObjectPrivate(handle, isOwner, qq)
    {
        Q_ASSERT(!map.contains(handle));
        map.insert(handle, qq);
        sc.connect(&handle->events.destroy, this, &QSessionLockManagerImplPrivate::on_destroy);
        sc.connect(&handle->events.lock, this, &QSessionLockManagerImplPrivate::on_lock);
    }

    ~QSessionLockManagerImplPrivate()
    {
        if (!m_handle)
            return;
        destroy();
    }

    inline void destroy()
    {
        Q_ASSERT(m_handle);
        Q_ASSERT(map.contains(m_handle));
        map.remove(m_handle);
        sc.invalidate();
    }

    void on_destroy(void *);
    void on_lock(void *);

    static QHash<void *, QSessionLockManagerImpl *> map;
    QW_DECLARE_PUBLIC(QSessionLockManagerImpl)
    QWSignalConnector sc;
};

QHash<void *, QSessionLockManagerImpl *> QSessionLockManagerImplPrivate::map;

void QSessionLockManagerImplPrivate::on_destroy(void *)
{
    destroy();
    m_handle = nullptr;
    delete q_func();
}

void QSessionLockManagerImplPrivate::on_lock(void *data)
{
    Q_Q(QSessionLockManagerImpl);
    auto *lock = QSessionLockImpl::from(static_cast<ext_session_lock_v1 *>(data));
    QObject::connect(lock,
                     &QSessionLockImpl::requestGetLockSurface,
                     q,
                     &QSessionLockManagerImpl::requestGetLockSurface);
    QObject::connect(lock,
                     &QSessionLockImpl::requestUnlock,
                     q,
                     &QSessionLockManagerImpl::requestUnlock);
}

QSessionLockManagerImpl::QSessionLockManagerImpl(ext_session_lock_manager_v1 *handle, bool isOwner)
    : QObject(nullptr)
    , QWObject(*new QSessionLockManagerImplPrivate(handle, isOwner, this))
{
}

QSessionLockManagerImpl *QSessionLockManagerImpl::get(ext_session_lock_manager_v1 *handle)
{
    return QSessionLockManagerImplPrivate::map.value(handle);
}

QSessionLockManagerImpl *QSessionLockManagerImpl::from(ext_session_lock_manager_v1 *handle)
{
    if (auto o = get(handle))
        return o;
    return new QSessionLockManagerImpl(handle, false);
}

QSessionLockManagerImpl *QSessionLockManagerImpl::create(QWDisplay *display)
{
    auto *handle = ext_session_lock_manager_v1_create(display->handle());
    if (!handle)
        return nullptr;
    return new QSessionLockManagerImpl(handle, true);
}

class QSessionLockImplPrivate : public QWObjectPrivate
{
public:
    QSessionLockImplPrivate(ext_session_lock_v1 *handle, bool isOwner, QSessionLockImpl *qq)
        : QWObjectPrivate(handle, isOwner, qq)
    {
        Q_ASSERT(!map.contains(handle));
        map.insert(handle, qq);
        sc.connect(&handle->events.get_lock_surface,
                   this,
                   &QSessionLockImplPrivate::on_get_lock_surface);
        sc.connect(&handle->events.unlock_and_destroy,
                   this,
                   &QSessionLockImplPrivate::on_unlock_and_destroy);
        sc.connect(&handle->events.destroy, this, &QSessionLockImplPrivate::on_destroy);
    }

    ~QSessionLockImplPrivate()
    {
        if (!m_handle)
            return;
        destroy();
        if (isHandleOwner) {
            ext_session_lock_v1_destroy(q_func()->handle());
        }
    }

    inline void destroy()
    {
        Q_ASSERT(m_handle);
        Q_ASSERT(map.contains(m_handle));
        map.remove(m_handle);
        sc.invalidate();
    }

    void on_get_lock_surface(void *);
    void on_unlock_and_destroy(void *);
    void on_destroy(void *);

    static QHash<void *, QSessionLockImpl *> map;
    QW_DECLARE_PUBLIC(QSessionLockImpl)
    QWSignalConnector sc;
    bool session_locked = false;
};

QHash<void *, QSessionLockImpl *> QSessionLockImplPrivate::map;

void QSessionLockImplPrivate::on_get_lock_surface(void *data)
{
    auto *lockSurface =
        QSessionLockSurfaceImpl::from(static_cast<ext_session_lock_surface_v1 *>(data));

    q_func()->requestGetLockSurface(lockSurface);
    q_func()->sendLockedToClient();
    session_locked = true;
}

void QSessionLockImplPrivate::on_unlock_and_destroy(void *data)
{
    auto *lock = QSessionLockImpl::from(static_cast<ext_session_lock_v1 *>(data));

    if (!session_locked) {
        wl_resource_post_error(q_func()->handle()->resource,
                               EXT_SESSION_LOCK_V1_ERROR_INVALID_UNLOCK,
                               "unlock requested but locked event was never sent");
        return;
    }

    q_func()->requestUnlock(lock);
    session_locked = false;
    destroy();
    m_handle = nullptr;
    delete q_func();
}

void QSessionLockImplPrivate::on_destroy(void *)
{
    if (!session_locked) {
        wl_resource_post_error(q_func()->handle()->resource,
                               EXT_SESSION_LOCK_V1_ERROR_INVALID_DESTROY,
                               "attempted to destroy session lock while locked");
        return;
    }

    destroy();
    m_handle = nullptr;
    delete q_func();
}

QSessionLockImpl::QSessionLockImpl(ext_session_lock_v1 *handle, bool isOwner)
    : QObject(nullptr)
    , QWObject(*new QSessionLockImplPrivate(handle, isOwner, this))
{
}

QSessionLockImpl *QSessionLockImpl::get(ext_session_lock_v1 *handle)
{
    return QSessionLockImplPrivate::map.value(handle);
}

QSessionLockImpl *QSessionLockImpl::from(ext_session_lock_v1 *handle)
{
    if (auto o = get(handle))
        return o;
    return new QSessionLockImpl(handle, false);
}

void QSessionLockImpl::sendLockedToClient()
{
    ext_session_lock_v1_send_locked(handle()->resource);
}

void QSessionLockImpl::sendFinishedToClient()
{
    ext_session_lock_v1_send_finished(handle()->resource);
}

class QSessionLockSurfaceImplPrivate : public QWObjectPrivate
{
public:
    QSessionLockSurfaceImplPrivate(ext_session_lock_surface_v1 *handle,
                                   bool isOwner,
                                   QSessionLockSurfaceImpl *qq)
        : QWObjectPrivate(handle, isOwner, qq)
    {
        Q_ASSERT(!surface_map.contains(handle));

        if (surface_map.contains(handle)) {
            wl_resource_post_error(q_func()->handle()->resource,
                                   EXT_SESSION_LOCK_V1_ERROR_DUPLICATE_OUTPUT,
                                   "given output already has a lock surface");
            return;
        }

        surface_map.insert(handle, qq);

        sc.connect(&handle->events.ack_configure,
                   this,
                   &QSessionLockSurfaceImplPrivate::on_ack_configure);
        sc.connect(&handle->events.destroy, this, &QSessionLockSurfaceImplPrivate::on_destroy);
    }

    ~QSessionLockSurfaceImplPrivate()
    {
        if (!m_handle)
            return;
        destroy();
        if (isHandleOwner) {
            ext_session_lock_surface_v1_destroy(q_func()->handle());
        }
    }

    inline void destroy()
    {
        Q_ASSERT(m_handle);
        Q_ASSERT(surface_map.contains(m_handle));
        surface_map.remove(m_handle);
        sc.invalidate();
    }

    void on_ack_configure(void *);
    void on_destroy(void *);

    static QHash<void *, QSessionLockSurfaceImpl *> surface_map;
    QW_DECLARE_PUBLIC(QSessionLockSurfaceImpl)
    QWSignalConnector sc;

    uint32_t configure_serial;
    uint32_t configure_width;
    uint32_t configure_height;
};

QHash<void *, QSessionLockSurfaceImpl *> QSessionLockSurfaceImplPrivate::surface_map;

void QSessionLockSurfaceImplPrivate::on_ack_configure(void *data)
{
    auto *d = QSessionLockSurfaceImpl::from(static_cast<ext_session_lock_surface_v1 *>(data));
    if (d->d_func()->configure_serial != configure_serial) {
        wl_resource_post_error(d->handle()->resource,
                               EXT_SESSION_LOCK_SURFACE_V1_ERROR_INVALID_SERIAL,
                               "erial provided in ack_configure is invalid");
        return;
    }

    if (d->d_func()->configure_width != configure_width
        || d->d_func()->configure_height != configure_height) {
        wl_resource_post_error(d->handle()->resource,
                               EXT_SESSION_LOCK_SURFACE_V1_ERROR_DIMENSIONS_MISMATCH,
                               "failed to match ack'd width/height");
        return;
    }
}

void QSessionLockSurfaceImplPrivate::on_destroy(void *)
{
    destroy();
    m_handle = nullptr;
    delete q_func();
}

#include "woutput.h"

WAYLIB_SERVER_USE_NAMESPACE
QSessionLockSurfaceImpl::QSessionLockSurfaceImpl(ext_session_lock_surface_v1 *handle, bool isOwner)
    : QObject(nullptr)
    , QWObject(*new QSessionLockSurfaceImplPrivate(handle, isOwner, this))
{
    WOutput *output = static_cast<WOutput *>(wl_resource_get_user_data(handle->output));
    sendConfigureToClient(handle->id,
                          output->nativeHandle()->width,
                          output->nativeHandle()->height);
}

QSessionLockSurfaceImpl *QSessionLockSurfaceImpl::get(ext_session_lock_surface_v1 *handle)
{
    return QSessionLockSurfaceImplPrivate::surface_map.value(handle);
}

QSessionLockSurfaceImpl *QSessionLockSurfaceImpl::from(ext_session_lock_surface_v1 *handle)
{
    if (auto o = get(handle))
        return o;
    return new QSessionLockSurfaceImpl(handle, false);
}

void QSessionLockSurfaceImpl::sendConfigureToClient(uint32_t serial,
                                                    uint32_t width,
                                                    uint32_t height)
{
    Q_D(QSessionLockSurfaceImpl);

    d->configure_serial = serial;
    d->configure_width = width;
    d->configure_height = height;
    ext_session_lock_surface_v1_send_configure(handle()->resource, serial, width, height);
}
