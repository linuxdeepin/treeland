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
class SurfaceWrapper;

class TreelandRegionWatchManagerInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT
public:
    explicit TreelandRegionWatchManagerInterfaceV1(QObject *parent = nullptr);
    ~TreelandRegionWatchManagerInterfaceV1() override;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

    // Recomputes the overlap state of every watcher against the given
    // per-window rect list (or, via recheck(), the internally tracked list).
    void checkOverlapConflict(const QList<QRect> &windowRects);

    // Track a non-layer surface whose geometry/visibility changes must
    // trigger a debounced overlap re-evaluation.
    void addSurface(SurfaceWrapper *wrapper);
    void removeSurface(SurfaceWrapper *wrapper);

    // Immediately re-evaluate every watcher against the currently tracked
    // window rects (set_region semantics and protocol tests).
    void recheck();

Q_SIGNALS:
    void regionWatchCreated(RegionWatchV1 *watch);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    void scheduleRecheck();

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

    TreelandRegionWatchManagerInterfaceV1 *manager() const;

private:
    explicit RegionWatchV1(wl_resource *resource);
    void onOutputDestroyed();

private:
    friend class TreelandRegionWatchManagerInterfaceV1Private;
    friend class RegionWatchV1Private;
    TreelandRegionWatchManagerInterfaceV1 *m_manager = nullptr;
    std::unique_ptr<RegionWatchV1Private> d;
};
