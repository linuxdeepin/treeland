// Copyright (C) 2024 ssk-wh <fanpengcheng@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "helper.h"
#include "qsessionlockmanagerimpl.h"

#include <wayland-server-protocol.h>
#include <wglobal.h>
#include <wquickwaylandserver.h>

class SessionLockManagerV1Private;

class SessionLockManagerV1 : public Waylib::Server::WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SessionLockManager)
    W_DECLARE_PRIVATE(SessionLockManagerV1)
    Q_PROPERTY(bool locked READ locked NOTIFY lockedChanged);

public:
    explicit SessionLockManagerV1(QObject *parent = nullptr);

    inline bool locked() const { return m_locked; }

Q_SIGNALS:
    void lockedChanged(bool);

protected:
    void create() override;

private:
    bool m_locked;
};
