// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"
#include "desktop/desktopenvironment.h"
#include "surface/surfacecontainer.h"

#ifndef DISABLE_XWAYLAND
#include <xcb/xproto.h>
#endif
#include <QtQml/qqmlregistration.h>
#include <QPointer>

class Workspace;
class SurfaceWrapper;
class TreelandUserConfig;
Q_MOC_INCLUDE("workspace/workspace.h")
Q_MOC_INCLUDE("treelanduserconfig.hpp")

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
Q_MOC_INCLUDE("session/sessionoutputmanager.h")
class SessionSwitcher;
Q_MOC_INCLUDE("session/sessionswitcher.h")
class InputManager;
class WallpaperManager;
Q_MOC_INCLUDE("input/inputmanager.h")
Q_MOC_INCLUDE("wallpaper/wallpapermanager.h")
#ifndef DISABLE_XWAYLAND
class SettingManager;
#endif
class WallpaperLauncher;
class ForeignToplevelManagerInterfaceV3;
Q_MOC_INCLUDE("modules/foreign-toplevel/foreigntoplevelmanagerv3.h")

class Session : public QObject {
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged FINAL)
public:
    ~Session();
    DesktopEnvironment *desktop() const;

    const QString &id() const;
    const QString &logindPath() const { return m_logindPath; }
    bool subscribeLogind(const QString &path);
    void unsubscribeLogind();
    uid_t uid() const;
    const QString &username() const;
    WSocket *socket() const;
#ifndef DISABLE_XWAYLAND
    WXWayland *xwayland() const;
    quint32 noTitlebarAtom() const;
#endif
    Workspace *workspace() const;
    DesktopSurfaceModel *windows() const;
    TreelandUserConfig *config() const;
    SessionSeatManager *seatManager() const;
    SessionProtocols *protocols() const;
    SessionPrelaunch *prelaunch() const;
    SessionScene *scene() const;
    SessionOutputManager *outputManager() const;
    SessionSwitcher *switcher() const;
    InputManager *inputManager() const;
    WallpaperManager *wallpaperManager() const;
    bool isActive() const;
    bool ownsClient(WClient *client) const;
    void attachInterface(WServerInterface *interface);
    void detachInterface(WServerInterface *interface);
    ForeignToplevelManagerInterfaceV3 *windowManagement() const;
    WForeignToplevel *foreignToplevel() const;
    WExtForeignToplevelListV1 *toplevelList() const;

    void registerWindow(SurfaceWrapper *window);
    void unregisterWindow(SurfaceWrapper *window);

Q_SIGNALS:
    void activeChanged(bool active);
    void aboutToBeDestroyed();
    void logindLockRequested(Session *session, bool locked);

private Q_SLOTS:
    void onLogindLock();
    void onLogindUnlock();

private:
    friend class SessionManager;
    friend class SurfaceProtocolController;

    bool updateLogindSubscription(const QString &path, bool subscribe);
    void setActive(bool active);
    DesktopEnvironment *m_desktop = nullptr;
    bool m_active = false;
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
    // Identity of this desktop; cleared with the Session on logout.
    QString m_id;
    QString m_logindPath;
    uid_t m_uid = 0;
    QString m_username = {};
    WSocket *m_socket = nullptr;
#ifndef DISABLE_XWAYLAND
    WXWayland *m_xwayland = nullptr;
    quint32 m_noTitlebarAtom = XCB_ATOM_NONE;
    SettingManager *m_settingManager = nullptr;
    QThread *m_settingManagerThread = nullptr;
#endif
};
