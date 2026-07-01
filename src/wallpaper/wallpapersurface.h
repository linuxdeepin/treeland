// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wsurface.h>
#include <wtoplevelsurface.h>

WAYLIB_SERVER_USE_NAMESPACE
QW_USE_NAMESPACE

class TreelandWallpaperSurfaceInterfaceV1;
class WallpaperSurfacePrivate;
class WAYLIB_SERVER_EXPORT WallpaperSurface : public WToplevelSurface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WallpaperSurface)
    Q_PROPERTY(WSurface *surface READ surface NOTIFY surfaceChanged)

    QML_NAMED_ELEMENT(WallpaperSurface)
    QML_UNCREATABLE("Only create in C++")

public:
    explicit WallpaperSurface(TreelandWallpaperSurfaceInterfaceV1 *handle,
                              QObject *parent = nullptr);
    ~WallpaperSurface();

    WSurface *surface() const override;
    QRect getContentGeometry() const override;

Q_SIGNALS:
    void surfaceChanged();

public Q_SLOTS:
    bool checkNewSize(const QSize &size, QSize *clipedSize = nullptr) override;

private:
    void init();
};
