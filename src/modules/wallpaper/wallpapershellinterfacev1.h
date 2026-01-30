// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wseat.h>
#include <wsurface.h>
#include <wserver.h>
#include <woutput.h>

#include <QObject>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class TreelandWallpaperSurfaceInterfaceV1;
class TreelandWallpaperShellInterfaceV1Private;
class TreelandWallpaperShellInterfaceV1 : public QObject , public WServerInterface
{
    Q_OBJECT
public:
    explicit TreelandWallpaperShellInterfaceV1(QObject *parent = nullptr);
    ~TreelandWallpaperShellInterfaceV1() override;

    static constexpr int InterfaceVersion = 1;

Q_SIGNALS:
    void wallpaperSurfaceAdded(TreelandWallpaperSurfaceInterfaceV1 *interface);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    std::unique_ptr<TreelandWallpaperShellInterfaceV1Private> d;
};

class TreelandWallpaperSurfaceInterfaceV1Private;
class TreelandWallpaperSurfaceInterfaceV1 : public QObject
{
    Q_OBJECT
public:
    ~TreelandWallpaperSurfaceInterfaceV1() override;
    WSurface *wSurface() const;
    QString source() const;
    wl_client *client();
    wl_resource *surfaceResource();

    static TreelandWallpaperSurfaceInterfaceV1 *get(WSurface *surface);
    static TreelandWallpaperSurfaceInterfaceV1 *get(const QString &source);

Q_SIGNALS:
    void failed(uint32_t error);
    void beforeDestroy(TreelandWallpaperSurfaceInterfaceV1 *surface);

private:
    explicit TreelandWallpaperSurfaceInterfaceV1(wl_resource *surface,
                                               const QString &source,
                                               wl_resource *resource);

private:
    friend class TreelandWallpaperShellInterfaceV1Private;
    std::unique_ptr<TreelandWallpaperSurfaceInterfaceV1Private> d;
};
