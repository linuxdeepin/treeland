// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wayland-server-core.h>
#include <wserver.h>

#include <qwbuffer.h>

#include <QObject>

#include <memory>

Q_MOC_INCLUDE(<qwbuffer.h>)

class PrelaunchSplashPrivate;

class PrelaunchSplash
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit PrelaunchSplash(QObject *parent = nullptr);
    ~PrelaunchSplash() override;

    QByteArrayView interfaceName() const override;
Q_SIGNALS:
    void splashRequested(const QString &appId,
                         const QString &instanceId,
                         QW_NAMESPACE::qw_buffer *iconBuffer);
    void splashCloseRequested(const QString &appId, const QString &instanceId);

protected: // WServerInterface
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<PrelaunchSplashPrivate> d;
};

Q_DECLARE_OPAQUE_POINTER(QW_NAMESPACE::qw_buffer *)
