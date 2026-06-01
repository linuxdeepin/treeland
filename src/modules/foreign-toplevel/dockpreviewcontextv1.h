// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "modules/foreign-toplevel/foreigntoplevelmanagerv1.h"

#include <QObject>
#include <QList>
#include <QPoint>
#include <QString>
#include <memory>

class ForeignToplevelHandleV1;
class ForeignToplevelManagerInterfaceV1;
class DockPreviewContextV1Private;

struct wl_resource;
struct wlr_surface;

class DockPreviewContextV1 : public QObject
{
    Q_OBJECT
public:
    ~DockPreviewContextV1() override;

    WSurface *relativeSurface() const;

    void enter();
    void leave();

Q_SIGNALS:
    void requestShow(const QPoint &pos,
                     ForeignToplevelManagerInterfaceV1::PreviewDirection direction,
                     const QList<ForeignToplevelHandleV1 *> &toplevels);
    void requestShowTooltip(const QString &tooltip,
                            const QPoint &pos,
                            ForeignToplevelManagerInterfaceV1::PreviewDirection direction);
    void requestClose();
    void beforeDestroy();

private:
    explicit DockPreviewContextV1(wl_resource *resource,
                                  wlr_surface *_relativeSurface,
                                  ForeignToplevelManagerInterfaceV1 *manager);

    wl_resource *resource() const;

    std::unique_ptr<DockPreviewContextV1Private> d;

    friend class DockPreviewContextV1Private;
    friend class ForeignToplevelManagerInterfaceV1Private;
};
