// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>

#include <wayland-server-core.h>
#include <wserver.h>
#include <qwbuffer.h>

#include <memory>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

namespace WAYLIB_SERVER_NAMESPACE {
class WServer;
}

QW_BEGIN_NAMESPACE
class qw_display;
class qw_buffer;
QW_END_NAMESPACE
Q_DECLARE_OPAQUE_POINTER(QW_NAMESPACE::qw_buffer*)

class PrelaunchSplashPrivate;
struct wl_global;

class PrelaunchSplash
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT
public:
    explicit PrelaunchSplash(QObject *parent = nullptr);
    ~PrelaunchSplash() override;

Q_SIGNALS:
    void splashRequested(const QString &appId, QW_NAMESPACE::qw_buffer *iconBuffer);

protected: // WServerInterface
    void create(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    void destroy(WAYLIB_SERVER_NAMESPACE::WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    std::unique_ptr<PrelaunchSplashPrivate> d;
};
