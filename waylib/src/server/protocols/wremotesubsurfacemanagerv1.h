// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServerInterface>

#include <QObject>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WRemoteSubsurfaceManagerV1Private;

class WAYLIB_SERVER_EXPORT WRemoteSubsurfaceManagerV1
    : public QObject
    , public WObject
    , public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WRemoteSubsurfaceManagerV1)

public:
    explicit WRemoteSubsurfaceManagerV1();
    ~WRemoteSubsurfaceManagerV1() override;

    static constexpr int InterfaceVersion = 1;
    QByteArrayView interfaceName() const override;

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
