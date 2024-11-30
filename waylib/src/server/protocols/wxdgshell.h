// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WServer>

class wlr_xdg_popup;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXdgToplevelSurface;
class WXdgPopupSurface;
class WXdgShellPrivate;
class WAYLIB_SERVER_EXPORT WXdgShell : public QObject, public WObject, public WServerInterface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WXdgShell)
public:
    WXdgShell(uint32_t version);

    QVector<WXdgToplevelSurface*> toplevelSurfaceList() const;
    QByteArrayView interfaceName() const override;

Q_SIGNALS:
    void toplevelSurfaceAdded(WXdgToplevelSurface *surface);
    void toplevelSurfaceRemoved(WXdgToplevelSurface *surface);
    void popupSurfaceAdded(WXdgPopupSurface *surface);
    void popupSurfaceRemoved(WXdgPopupSurface *surface);

public:
    void initializeNewXdgPopupSurface(wlr_xdg_popup *popup);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
};

WAYLIB_SERVER_END_NAMESPACE
