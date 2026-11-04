// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include "modules/foreign-toplevel/foreigntoplevelmanagerv3.h"

#include <QObject>
#include <QList>
#include <QPoint>
#include <QString>
#include <memory>

Q_MOC_INCLUDE("modules/foreign-toplevel/foreigntoplevelhandlev3.h")

class ForeignToplevelHandleV3;
class ForeignToplevelManagerInterfaceV3;
class DockPreviewContextV3Private;

struct wl_resource;
class DockPreviewContextV3 : public QObject
{
    Q_OBJECT
public:
    ~DockPreviewContextV3() override;

    WSurface *relativeSurface() const;

    void enter();
    void leave();

Q_SIGNALS:
    void requestShow(const QPoint &pos,
                     ForeignToplevelManagerInterfaceV3::PreviewDirection direction,
                     const QList<ForeignToplevelHandleV3 *> &toplevels);
    void requestShowTooltip(WSurface *surface,
                            const QPoint &pos,
                            ForeignToplevelManagerInterfaceV3::PreviewDirection direction);
    void requestClose();
    void beforeDestroy();

private:
    explicit DockPreviewContextV3(wl_resource *resource,
                                  wlr_surface *_relativeSurface,
                                  ForeignToplevelManagerInterfaceV3 *manager);

    wl_resource *resource() const;

    std::unique_ptr<DockPreviewContextV3Private> d;

    friend class DockPreviewContextV3Private;
    friend class ForeignToplevelManagerInterfaceV3Private;
};
