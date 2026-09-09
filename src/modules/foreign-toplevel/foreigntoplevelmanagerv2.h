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
class DockPreviewContextV2;
class ForeignToplevelHandleV2;
class ForeignToplevelManagerInterfaceV2Private;

WAYLIB_SERVER_USE_NAMESPACE

class ForeignToplevelManagerInterfaceV2
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

    explicit ForeignToplevelManagerInterfaceV2(QObject *parent = nullptr);
    ~ForeignToplevelManagerInterfaceV2() override;

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
    void requestDockPreviewTooltip(QString tooltip,
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
    ForeignToplevelHandleV2 *handleForIdentifier(uint32_t identifier) const;
    void releaseHandle(ForeignToplevelHandleV2 *handle);
    void releaseDockPreviewContext(DockPreviewContextV2 *context);
    void initializeToplevelHandle(SurfaceWrapper *wrapper, ForeignToplevelHandleV2 *handle);

    std::unique_ptr<ForeignToplevelManagerInterfaceV2Private> d;

    friend class ForeignToplevelManagerInterfaceV2Private;
    friend class DockPreviewContextV2;
    friend class DockPreviewContextV2Private;
    friend class ForeignToplevelHandleV2Private;
};
