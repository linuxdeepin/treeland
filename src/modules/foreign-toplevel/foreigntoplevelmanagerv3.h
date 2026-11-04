// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <wsurface.h>

#include <QPoint>
#include <QObject>
#include <QString>
#include <memory>
#include <vector>

class SurfaceWrapper;
class DockPreviewContextV3;
class ForeignToplevelHandleV3;
class ForeignToplevelManagerInterfaceV3Private;

WAYLIB_SERVER_USE_NAMESPACE

class ForeignToplevelManagerInterfaceV3
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

public:
    enum class PreviewDirection
    {
        top = 0,
        right,
        bottom,
        left,
    };
    Q_ENUM(PreviewDirection)

    explicit ForeignToplevelManagerInterfaceV3(QObject *parent = nullptr);
    ~ForeignToplevelManagerInterfaceV3() override;

    void addSurface(SurfaceWrapper *wrapper);
    void removeSurface(SurfaceWrapper *wrapper);

    Q_INVOKABLE void enterDockPreview(WSurface *relativeSurface);
    Q_INVOKABLE void leaveDockPreview(WSurface *relativeSurface);

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;

Q_SIGNALS:
    void requestDockPreview(std::vector<SurfaceWrapper *> surfaces,
                            WSurface *target,
                            QPoint abs,
                            PreviewDirection direction);
    void requestDockPreviewTooltip(WSurface *surface,
                                   WSurface *target,
                                   QPoint abs,
                                   PreviewDirection direction);
    void requestDockClose();

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    wl_event_loop *eventLoop() const;
    ForeignToplevelHandleV3 *handleForIdentifier(uint32_t identifier) const;
    void releaseHandle(ForeignToplevelHandleV3 *handle);
    void releaseDockPreviewContext(DockPreviewContextV3 *context);
    void initializeToplevelHandle(SurfaceWrapper *wrapper, ForeignToplevelHandleV3 *handle);

    std::unique_ptr<ForeignToplevelManagerInterfaceV3Private> d;

    friend class ForeignToplevelManagerInterfaceV3Private;
    friend class DockPreviewContextV3;
    friend class DockPreviewContextV3Private;
    friend class ForeignToplevelHandleV3Private;
};
