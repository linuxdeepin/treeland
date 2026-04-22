// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wayland-server-core.h>
#include <wglobal.h>
#include <wserver.h>

#include <QObject>

#include <memory>

class WineWindowManagementManagerPrivate;

class WineWindowManagementManager
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit WineWindowManagementManager(QObject *parent = nullptr);
    ~WineWindowManagementManager() override;

protected:
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    std::unique_ptr<WineWindowManagementManagerPrivate> d;
};
