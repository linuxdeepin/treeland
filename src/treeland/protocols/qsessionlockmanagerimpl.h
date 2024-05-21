// Copyright (C) 2024 ssk-wh <fanpengcheng@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "ext_session_lock_manager_impl.h"
#include "qqmlintegration.h"

#include <qwglobal.h>

#include <QObject>

QW_USE_NAMESPACE

QW_BEGIN_NAMESPACE
class QWOutput;
class QWDisplay;
QW_END_NAMESPACE

class QSessionLockImpl;
class QSessionLockSurfaceImpl;
class QSessionLockManagerImplPrivate;

class QSessionLockManagerImpl : public QObject, public QWObject
{
    Q_OBJECT
    QW_DECLARE_PRIVATE(QSessionLockManagerImpl)
public:
    inline ext_session_lock_manager_v1 *handle() const
    {
        return QWObject::handle<ext_session_lock_manager_v1>();
    }

    static QSessionLockManagerImpl *get(ext_session_lock_manager_v1 *handle);
    static QSessionLockManagerImpl *from(ext_session_lock_manager_v1 *handle);
    static QSessionLockManagerImpl *create(QWDisplay *display);

Q_SIGNALS:
    void requestGetLockSurface(QSessionLockSurfaceImpl *);
    void requestUnlock(QSessionLockImpl *);

private:
    QSessionLockManagerImpl(ext_session_lock_manager_v1 *handle, bool isOwner);
    ~QSessionLockManagerImpl() = default;
};

class QSessionLockImplPrivate;

class QSessionLockImpl : public QObject, public QWObject
{
    Q_OBJECT
    QML_ELEMENT
    QW_DECLARE_PRIVATE(QSessionLockImpl)
public:
    ~QSessionLockImpl() = default;

    inline ext_session_lock_v1 *handle() const { return QWObject::handle<ext_session_lock_v1>(); }

    static QSessionLockImpl *get(ext_session_lock_v1 *handle);
    static QSessionLockImpl *from(ext_session_lock_v1 *handle);

    void sendLockedToClient();
    void sendFinishedToClient();

Q_SIGNALS:
    void requestGetLockSurface(QSessionLockSurfaceImpl *);
    void requestUnlock(QSessionLockImpl *);

private:
    QSessionLockImpl(ext_session_lock_v1 *handle, bool isOwner);
};

class QSessionLockSurfaceImplPrivate;

class QSessionLockSurfaceImpl : public QObject, public QWObject
{
    Q_OBJECT
    QW_DECLARE_PRIVATE(QSessionLockSurfaceImpl)

public:
    ~QSessionLockSurfaceImpl() = default;

    inline struct ext_session_lock_surface_v1 *handle() const
    {
        return QWObject::handle<struct ext_session_lock_surface_v1>();
    }

    static QSessionLockSurfaceImpl *get(ext_session_lock_surface_v1 *handle);
    static QSessionLockSurfaceImpl *from(ext_session_lock_surface_v1 *handle);

    void sendConfigureToClient(uint32_t serial, uint32_t width, uint32_t height);

private:
    QSessionLockSurfaceImpl(ext_session_lock_surface_v1 *handle, bool isOwner);
};
