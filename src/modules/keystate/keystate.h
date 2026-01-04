// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wserver.h"

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSeat;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE


class KeyStateV5Private;
class KeyStateV5
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
public:
    explicit KeyStateV5(WSeat *seat, QObject *parent = nullptr);
    ~KeyStateV5() override;

    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<KeyStateV5Private> d;
};
