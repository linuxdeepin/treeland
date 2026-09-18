// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>

#include <wayland-server-core.h>

#include <QRect>
#include <QObject>

WAYLIB_SERVER_USE_NAMESPACE

class RegionWatchV1Private;
class RegionWatchV1;
class TreelandRegionWatchManagerInterfaceV1Private;

class TreelandRegionWatchManagerInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT
public:
    explicit TreelandRegionWatchManagerInterfaceV1(QObject *parent = nullptr);
    ~TreelandRegionWatchManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

    // Recomputes the overlap state of every watcher against the per-window
    // rect list tracked by Helper.
    void checkOverlapConflict(const QList<QRect> &windowRects);

Q_SIGNALS:
    void regionWatchCreated(RegionWatchV1 *watch);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<TreelandRegionWatchManagerInterfaceV1Private> d;
};

class RegionWatchV1 : public QObject
{
    Q_OBJECT
public:
    enum Anchor {
        Top = 0,
        Bottom = 1,
        Left = 2,
        Right = 3,
    };
    Q_ENUM(Anchor)

    ~RegionWatchV1() override;

    // Monitored region in global layout coordinates (output position +
    // strip); empty when the watcher is
    // inert (no successful set_region yet, or the associated output was
    // removed).
    QRect region() const;

private:
    explicit RegionWatchV1(wl_resource *resource);

private:
    friend class TreelandRegionWatchManagerInterfaceV1Private;
    std::unique_ptr<RegionWatchV1Private> d;
};
