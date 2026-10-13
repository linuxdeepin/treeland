// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wxdgdecorationmanager.h>

#include <QObject>

#include <sys/types.h>

class WaylandServer;
class ShellHandler;
class SurfaceWrapper;
class PersonalizationManagerInterfaceV1;
class DecorationManagerInterfaceV1;
class TreelandRegionWatchManagerInterfaceV1;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServer;
class WForeignToplevel;
class WExtForeignToplevelListV1;
class WXdgDialogManagerV1;
class WXdgToplevelTagManagerV1;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class SurfaceProtocolController : public QObject
{
    Q_OBJECT
public:
    explicit SurfaceProtocolController(WaylandServer *server);

    void installRegionWatch(WServer *server);
    void installToplevels(WServer *server, ShellHandler *shell);
    void installPersonalization(WServer *server);
    void installDecoration(WServer *server);
    void installDecorationCustomization(WServer *server);
    void setUserId(uid_t uid);
    WExtForeignToplevelListV1 *toplevelList() const;
private:
    void onSurfaceModeChanged(WSurface *surface, WXdgDecorationManager::DecorationMode mode);
    void onSurfaceWrapperAdded(SurfaceWrapper *wrapper);
    void onSurfaceWrapperAboutToRemove(SurfaceWrapper *wrapper);

private:
    WaylandServer *m_server = nullptr;
    WForeignToplevel *m_foreignToplevel = nullptr;
    WExtForeignToplevelListV1 *m_extForeignToplevelListV1 = nullptr;
    WXdgDecorationManager *m_xdgDecorationManager = nullptr;
    WXdgDialogManagerV1 *m_xdgDialogManagerV1 = nullptr;
    WXdgToplevelTagManagerV1 *m_xdgToplevelTagManagerV1 = nullptr;
    PersonalizationManagerInterfaceV1 *m_personalizationInterfaceV1 = nullptr;
    DecorationManagerInterfaceV1 *m_decorationInterfaceV1 = nullptr;
    TreelandRegionWatchManagerInterfaceV1 *m_regionWatchManagerInterfaceV1 = nullptr;
};
