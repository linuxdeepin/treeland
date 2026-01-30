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

class TreelandWallpaperInterfaceV1;
class TreelandWallpaperManagerInterfaceV1Private;
class TreelandWallpaperManagerInterfaceV1 : public QObject , public WServerInterface
{
    Q_OBJECT
public:
    explicit TreelandWallpaperManagerInterfaceV1(QObject *parent = nullptr);
    ~TreelandWallpaperManagerInterfaceV1() override;

    static constexpr int InterfaceVersion = 1;

Q_SIGNALS:
    void wallpaperCreated(TreelandWallpaperInterfaceV1 *interface);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    QByteArrayView interfaceName() const override;

private:
    std::unique_ptr<TreelandWallpaperManagerInterfaceV1Private> d;
};

class TreelandWallpaperInterfaceV1Private;
class TreelandWallpaperInterfaceV1 : public QObject
{
    Q_OBJECT
public:
    enum Error {
        AlreadyUsed = 0,
        InvalidSource = 1,
        PermissionDenied = 2,
    };

    enum WallpaperType {
        Image = 0,
        Video = 1,
    };

    enum WallpaperRole {
        Desktop    = 0x1,
        Lockscreen = 0x2
    };
    Q_DECLARE_FLAGS(WallpaperRoles, WallpaperRole)
    Q_FLAG(WallpaperRoles)

    ~TreelandWallpaperInterfaceV1() override;

    WSurface *referenceWSurface() const;
    QString userName() const;
    WOutput *wOutput() const;

    void sendError(const QString &source, Error error);
    void sendChanged(WallpaperRoles roles, WallpaperType type, const QString &source);

    static TreelandWallpaperInterfaceV1 *getReferenceWallpaperInterfaceFromSurface(WSurface *surface);

Q_SIGNALS:
    void imageSourceChanged(int workspaceIndex,
                            const QString &fileSource,
                            WallpaperRoles roles);
    void videoSourceChanged(int workspaceIndex,
                            const QString &fileSource,
                            WallpaperRoles roles);
    void binded();

private:
    explicit TreelandWallpaperInterfaceV1(struct wl_resource *output,
                                          const QString &userName,
                                          wl_resource *resource,
                                          wl_resource *refsurface,
                                          QObject *parent = nullptr);

private:
    friend class TreelandWallpaperManagerInterfaceV1Private;
    std::unique_ptr<TreelandWallpaperInterfaceV1Private> d;
};
