// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wxdgdecorationmanager.h>

#include <QObject>

#include <sys/types.h>

class WaylandServer;
class Session;
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
    void installToplevels(ShellHandler *shell);
    void initializeSession(Session *session);
private:
    void onSurfaceModeChanged(WSurface *surface, WXdgDecorationManager::DecorationMode mode);
    void connectForeignToplevel(WForeignToplevel *manager);
    void addForeignToplevel(SurfaceWrapper *wrapper);
    void removeForeignToplevel(SurfaceWrapper *wrapper);
    void onSurfaceWrapperAdded(SurfaceWrapper *wrapper);
    void onSurfaceWrapperAboutToRemove(SurfaceWrapper *wrapper);

private:
    WaylandServer *m_server = nullptr;
    TreelandRegionWatchManagerInterfaceV1 *m_regionWatchManagerInterfaceV1 = nullptr;
};
