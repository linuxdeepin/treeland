// Copyright (C) 2025 misaka18931 <miruku2937@gmail.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"
#include "wserver.h"

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSessionLock;
class WSessionLockManagerPrivate;
class WAYLIB_SERVER_EXPORT WSessionLockManager : public WWrapObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WSessionLockManager)
public:
    explicit WSessionLockManager(QObject *parent = nullptr);
    void *create();

    QByteArrayView interfaceName() const override;
    QVector<WSessionLock*> lockList() const;
Q_SIGNALS:
    void lockCreated(WSessionLock *lock);
    void lockDestroyed(WSessionLock *lock);
protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
