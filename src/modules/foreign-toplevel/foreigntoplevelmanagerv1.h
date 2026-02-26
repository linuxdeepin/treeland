// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "modules/foreign-toplevel/impl/foreign_toplevel_manager_impl.h"
#include "treeland-foreign-toplevel-manager-protocol.h"

#include <wserver.h>
#include <wsurface.h>
#include <wxdgsurface.h>

class SurfaceWrapper;

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class ForeignToplevelV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

public:
    enum class PreviewDirection
    {
        top = TREELAND_DOCK_PREVIEW_CONTEXT_V1_DIRECTION_TOP,
        right = TREELAND_DOCK_PREVIEW_CONTEXT_V1_DIRECTION_RIGHT,
        bottom = TREELAND_DOCK_PREVIEW_CONTEXT_V1_DIRECTION_BOTTOM,
        left = TREELAND_DOCK_PREVIEW_CONTEXT_V1_DIRECTION_LEFT,
    };
    Q_ENUM(PreviewDirection);

    explicit ForeignToplevelV1(QObject *parent = nullptr);

    void addSurface(SurfaceWrapper *wrapper);
    void removeSurface(SurfaceWrapper *wrapper);

    Q_INVOKABLE void enterDockPreview(WSurface *relative_surface);
    Q_INVOKABLE void leaveDockPreview(WSurface *relative_surface);

    QByteArrayView interfaceName() const override;

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
    wl_global *global() const override;

private Q_SLOTS:
    void onDockPreviewContextCreated(treeland_dock_preview_context_v1 *context);

private:
    void initializeToplevelHandle(SurfaceWrapper *wrapper,
                                  treeland_foreign_toplevel_handle_v1 *handle);

    treeland_foreign_toplevel_manager_v1 *m_manager = nullptr;
    std::map<SurfaceWrapper *, std::unique_ptr<treeland_foreign_toplevel_handle_v1>> m_surfaces;
    uint32_t m_nextIdentifier = 1;
};

Q_DECLARE_OPAQUE_POINTER(treeland_foreign_toplevel_handle_v1_maximized_event *);
Q_DECLARE_OPAQUE_POINTER(treeland_foreign_toplevel_handle_v1_minimized_event *);
Q_DECLARE_OPAQUE_POINTER(treeland_foreign_toplevel_handle_v1_activated_event *);
Q_DECLARE_OPAQUE_POINTER(treeland_foreign_toplevel_handle_v1_fullscreen_event *);
Q_DECLARE_OPAQUE_POINTER(treeland_foreign_toplevel_handle_v1_set_rectangle_event *);
