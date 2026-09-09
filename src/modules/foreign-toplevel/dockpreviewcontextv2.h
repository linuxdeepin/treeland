// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include "modules/foreign-toplevel/foreigntoplevelmanagerv2.h"

#include <QObject>
#include <QList>
#include <QPoint>
#include <QString>
#include <memory>

Q_MOC_INCLUDE("modules/foreign-toplevel/foreigntoplevelhandlev2.h")

class ForeignToplevelHandleV2;
class ForeignToplevelManagerInterfaceV2;
class DockPreviewContextV2Private;

struct wl_resource;
class DockPreviewContextV2 : public QObject
{
    Q_OBJECT
public:
    ~DockPreviewContextV2() override;

    WSurface *relativeSurface() const;

    void enter();
    void leave();

Q_SIGNALS:
    void requestShow(const QPoint &pos,
                     ForeignToplevelManagerInterfaceV2::PreviewDirection direction,
                     const QList<ForeignToplevelHandleV2 *> &toplevels);
    void requestShowTooltip(const QString &tooltip,
                            const QPoint &pos,
                            ForeignToplevelManagerInterfaceV2::PreviewDirection direction);
    void requestClose();
    void beforeDestroy();

private:
    explicit DockPreviewContextV2(wl_resource *resource,
                                  wlr_surface *_relativeSurface,
                                  ForeignToplevelManagerInterfaceV2 *manager);

    wl_resource *resource() const;

    std::unique_ptr<DockPreviewContextV2Private> d;

    friend class DockPreviewContextV2Private;
    friend class ForeignToplevelManagerInterfaceV2Private;
};
