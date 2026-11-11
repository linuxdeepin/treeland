// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"
#include "desktop/desktopenvironment.h"

#include <QtQml/qqmlregistration.h>
#include <QPointer>

#ifndef DISABLE_XWAYLAND
#include <xcb/xproto.h>
#endif

Q_MOC_INCLUDE("workspace/workspace.h")
Q_MOC_INCLUDE("treelanduserconfig.hpp")
Q_MOC_INCLUDE("session/sessionoutputmanager.h")
Q_MOC_INCLUDE("session/sessionswitcher.h")
Q_MOC_INCLUDE("input/inputmanager.h")
Q_MOC_INCLUDE("wallpaper/wallpapermanager.h")

class Workspace;
class SurfaceWrapper;
class TreelandUserConfig;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WServerInterface;
class WClient;
class WSocket;
#ifndef DISABLE_XWAYLAND
class WXWayland;
#endif
class WForeignToplevel;
class WExtForeignToplevelListV1;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class SessionSeatManager;
class SessionProtocols;
class SessionPrelaunch;
class SessionScene;
class SessionOutputManager;
class SessionSwitcher;
class InputManager;
class WallpaperManager;
#ifndef DISABLE_XWAYLAND
class SettingManager;
#endif
class WallpaperLauncher;
class ForeignToplevelManagerInterfaceV3;

class Session : public QObject {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged FINAL)
public:
    ~Session();

    DesktopEnvironment *desktop() const { return m_desktop; };
    const QString &id() const { return m_id; };
    uid_t uid() const { return m_uid; };
    const QString &username() const { return m_username; };
    WSocket *socket() const { return m_socket; };
#ifndef DISABLE_XWAYLAND
    WXWayland *xwayland() const { return m_xwayland; };
    quint32 noTitlebarAtom() const { return m_noTitlebarAtom; };
#endif
    Workspace *workspace() const { return m_desktop->workspace(); };
    DesktopSurfaceModel *windows() const { return m_desktop->windows(); };
    TreelandUserConfig *config() const { return m_config; };
    SessionSeatManager *seatManager() const { return m_seatManager; };
    SessionProtocols *protocols() const { return m_protocols; };
    SessionPrelaunch *prelaunch() const { return m_prelaunch; };
    SessionScene *scene() const { return m_scene; };
    SessionOutputManager *outputManager() const { return m_outputManager; };
    SessionSwitcher *switcher() const { return m_desktop->switcher(); };
    InputManager *inputManager() const { return m_desktop->input(); };
    WallpaperManager *wallpaperManager() const { return m_desktop->wallpaper(); };

    ForeignToplevelManagerInterfaceV3 *windowManagement() const { return m_windowManagement; };
    WForeignToplevel *foreignToplevel() const;
    WExtForeignToplevelListV1 *toplevelList() const;

    bool isActive() const { return m_active; };
    bool ownsClient(WClient *client) const;
    void attachInterface(WServerInterface *interface);
    void detachInterface(WServerInterface *interface);

    void registerWindow(SurfaceWrapper *window);
    void unregisterWindow(SurfaceWrapper *window);

Q_SIGNALS:
    void activeChanged(bool active);
    void inputMethodChanged(bool connected);
    void aboutToBeDestroyed();

private:
    void setActive(bool active);

private:
    friend class SessionManager;
    friend class SurfaceProtocolController;

    DesktopEnvironment *m_desktop = nullptr;
    SessionSeatManager *m_seatManager = nullptr;
    SessionProtocols *m_protocols = nullptr;
    SessionPrelaunch *m_prelaunch = nullptr;
    SessionScene *m_scene = nullptr;
    SessionOutputManager *m_outputManager = nullptr;
    QList<WServerInterface *> m_interfaces;
    QPointer<ForeignToplevelManagerInterfaceV3> m_windowManagement;
    QPointer<WForeignToplevel> m_foreignToplevel;
    QPointer<WExtForeignToplevelListV1> m_toplevelList;
    TreelandUserConfig *m_config = nullptr;
    WSocket *m_socket = nullptr;
    QString m_id;
    uid_t m_uid = 0;
    QString m_username = {};
#ifndef DISABLE_XWAYLAND
    WXWayland *m_xwayland = nullptr;
    quint32 m_noTitlebarAtom = XCB_ATOM_NONE;
    SettingManager *m_settingManager = nullptr;
    QThread *m_settingManagerThread = nullptr;
#endif
    bool m_active = false;
};
