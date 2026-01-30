// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wallpapermanagerinterfacev1.h"

#include <wseat.h>
#include <wsurface.h>
#include <wserver.h>
#include <woutput.h>

#include <QObject>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class TreelandWallpaperNotifierInterfaceV1Private;
class TreelandWallpaperNotifierInterfaceV1 : public QObject , public WServerInterface
{
    Q_OBJECT
public:
    explicit TreelandWallpaperNotifierInterfaceV1(QObject *parent = nullptr);
    ~TreelandWallpaperNotifierInterfaceV1() override;

    static constexpr int InterfaceVersion = 1;

    void sendAdd(TreelandWallpaperInterfaceV1::WallpaperType type, const QString &fileSource);
    void sendRemove(const QString &fileSource);

Q_SIGNALS:
    void binded();

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    std::unique_ptr<TreelandWallpaperNotifierInterfaceV1Private> d;
};
