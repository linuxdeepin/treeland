// Copyright (C) 2025 misaka18931 <miruku2937@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include "wglobal.h"

Q_MOC_INCLUDE("wsessionlocksurface.h")

QW_BEGIN_NAMESPACE
class qw_session_lock_v1;
QW_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSessionLockSurface;
class WSessionLockPrivate;

class WAYLIB_SERVER_EXPORT WSessionLock : public WWrapObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WSessionLock)

    Q_PROPERTY(LockState locked READ lockState);

    QML_NAMED_ELEMENT(WSessionLock)
    QML_UNCREATABLE("Only create in C++")
public:
    enum class LockState {
        Created,
        Locked,
        Finished,
        Unlocked,
        Abandoned,
        Canceled
    };
    explicit WSessionLock(qw_session_lock_v1 *handle, QObject *parent = nullptr);
    ~WSessionLock();
    static WSessionLock *fromHandle(qw_session_lock_v1 *handle);

    Q_INVOKABLE void lock();
    Q_INVOKABLE void finish();
    LockState lockState() const;
    bool isLocked() const;
    qw_session_lock_v1 *handle() const;
    
    QVector<WSessionLockSurface*> surfaceList() const;
Q_SIGNALS:
    void surfaceAdded(WSessionLockSurface *surface);
    void surfaceRemoved(WSessionLockSurface *surface);
    void locked();
    void unlocked();
    void finished();
    void abandoned();
    void canceled();
};

WAYLIB_SERVER_END_NAMESPACE
