// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <QQmlEngine>

WAYLIB_SERVER_USE_NAMESPACE

class WallpaperColorInterfaceV1Private;
class WallpaperColorInterfaceV1
    : public QObject
    , public WAYLIB_SERVER_NAMESPACE::WServerInterface
{
    Q_OBJECT

public:
    explicit WallpaperColorInterfaceV1(QObject *parent = nullptr);
    ~WallpaperColorInterfaceV1() override;

    static constexpr int InterfaceVersion = 1;
    Q_INVOKABLE void updateWallpaperColor(const QString &output, bool isDarkType);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    std::unique_ptr<WallpaperColorInterfaceV1Private> d;
};
