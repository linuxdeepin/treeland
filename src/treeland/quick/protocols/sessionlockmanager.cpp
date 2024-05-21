// Copyright (C) 2024 ssk-wh <fanpengcheng@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sessionlockmanager.h"

#include "qsessionlockmanagerimpl.h"

class SessionLockManagerV1Private : public WObjectPrivate
{
public:
    SessionLockManagerV1Private(SessionLockManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    ~SessionLockManagerV1Private() = default;

    W_DECLARE_PUBLIC(SessionLockManagerV1)

    QSessionLockManagerImpl *manager = nullptr;
};

SessionLockManagerV1::SessionLockManagerV1(QObject *parent)
    : Waylib::Server::WQuickWaylandServerInterface(parent)
    , WObject(*new SessionLockManagerV1Private(this), nullptr)
    , m_locked(false)
{
}

void SessionLockManagerV1::create()
{
    W_D(SessionLockManagerV1);

    d->manager = QSessionLockManagerImpl::create(server()->handle());
    connect(d->manager, &QSessionLockManagerImpl::requestGetLockSurface, this, [this] {
        m_locked = true;
        Q_EMIT lockedChanged(m_locked);
    });
    connect(d->manager, &QSessionLockManagerImpl::requestUnlock, this, [this] {
        m_locked = false;
        Q_EMIT lockedChanged(m_locked);
    });
}
