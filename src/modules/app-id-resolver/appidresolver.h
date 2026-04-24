// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wayland-server-core.h>
#include <wglobal.h>
#include <wserver.h>

#include <QObject>

#include <functional>
#include <memory>

class AppIdResolverManagerPrivate;

class AppIdResolverManager
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit AppIdResolverManager(QObject *parent = nullptr);
    ~AppIdResolverManager() override;

    // Callback-based API: returns true if request started, false if no resolver or dup fd failed
    // Callback is invoked asynchronously on the Wayland (main) thread; empty string if resolver
    // disconnects
    bool resolvePidfd(int pidfd, std::function<void(const QString &)> callback);
    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void availableChanged();

protected: // WServerInterface overrides
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<AppIdResolverManagerPrivate> d;
};
