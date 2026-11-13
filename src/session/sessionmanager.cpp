// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "sessionmanager.h"
#include "sessionprelaunch.h"
#include "sessionscene.h"
#include "session.h"
#include "desktop/desktopenvironment.h"
#include "seatuserconfig.hpp"
#include "sessionprotocols.h"
#include "sessionseatmanager.h"
#include "sessionswitcher.h"
#include "sessionoutputmanager.h"
#include "sessionoutputchannel.h"
#include "sessioninputchannel.h"
#include "outputconfig.hpp"
#include <woutputmanagerv1.h>
#include "dconfig/dconfigmanager.h"
#include "input/inputmanager.h"
#include "wallpaper/wallpapermanager.h"
#include "surface/surfacecontainer.h"
#include "seat/seatmanager.h"

#include "common/treelandlogging.h"
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "output/output.h"
#include "core/waylandserver.h"
#include "treelanduserconfig.hpp"
#include "wallpaper/wallpaperlauncher.h"
#include "workspace/workspace.h"
#ifndef DISABLE_XWAYLAND
#include "xsettings/settingmanager.h"
#endif

#include <woutputrenderwindow.h>
#include <wsocket.h>
#include <wforeigntoplevelv1.h>
#include <wserver.h>
#include "modules/foreign-toplevel/foreigntoplevelmanagerv3.h"
#include <wextforeigntoplevellistv1.h>
#ifndef DISABLE_XWAYLAND
#include <wxwayland.h>
#endif

#include "surface/surfacewrapper.h"

#include <pwd.h>
#include <wlr_all.h>
#ifndef DISABLE_XWAYLAND
#include <xcb/randr.h>
#endif

#ifndef DISABLE_XWAYLAND
#define _DEEPIN_NO_TITLEBAR "_DEEPIN_NO_TITLEBAR"

static void applySessionXSettings(SettingManager *settingManager,
                                 const QString &theme,
                                 qreal size,
                                 const QString &imModule)
{
    if (!settingManager) {
        qCDebug(lcTlXsettings) << "Skip XSettings sync: no SettingManager";
        return;
    }

    const bool queued = QMetaObject::invokeMethod(
        settingManager,
        [settingManager, theme, size, imModule]() {
            settingManager->setCursorTheme(theme);
            settingManager->setCursorSize(size);
            settingManager->setIMModule(imModule);
            settingManager->apply();
        },
        Qt::QueuedConnection);
    if (!queued) {
        qCWarning(lcTlXsettings) << "Failed to queue XSettings sync"
                                     << "theme:" << theme << "size:" << size
                                     << "imModule:" << imModule;
    }
}

static xcb_atom_t internAtom(xcb_connection_t *connection, const char *name, bool onlyIfExists)
{
    if (!name || *name == 0)
        return XCB_NONE;

    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, onlyIfExists, strlen(name), name);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(connection, cookie, 0);

    if (!reply)
        return XCB_NONE;

    xcb_atom_t atom = reply->atom;
    free(reply);

    return atom;
}
#endif

Session *SessionManager::activeSessionObject() const
{
    return m_activeSession.lock().get();
}

std::shared_ptr<Session> SessionManager::sessionForClient(WClient *client) const
{
    if (!client)
        return nullptr;
    for (auto *socket = client->socket(); socket; socket = socket->parentSocket()) {
        if (auto session = sessionForSocket(socket))
            return session;
    }
    return nullptr;
}

bool SessionManager::isActiveWindow(SurfaceWrapper *window) const
{
    if (!window)
        return false;
    if (window->session())
        return window->renderingEnabled();
    return !activeSessionObject();
}

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
}

SessionManager::~SessionManager()
{
    m_sessions.clear();
    if (m_wallpaperLauncher) {
        delete m_wallpaperLauncher;
        m_wallpaperLauncher = nullptr;
    }
}

void SessionManager::syncActiveSessionXWaylandPrimaryOutput()
{
#ifndef DISABLE_XWAYLAND
    const auto *primaryOutput = WaylandServer::instance()->rootSurfaceContainer()->primaryOutput();
    if (!primaryOutput)
        return;

    auto session = m_activeSession.lock();
    if (!session)
        return;

    const QString primaryOutputName = primaryOutput->output()->name();
    auto *xwayland = session->xwayland();
    auto *connection = xwayland ? xwayland->xcbConnection() : nullptr;
    auto *screen = xwayland ? xwayland->xcbScreen() : nullptr;
    if (!connection || !screen)
        return;

    const auto resourcesCookie =
        xcb_randr_get_screen_resources_current(connection, screen->root);
    auto *resources =
        xcb_randr_get_screen_resources_current_reply(connection, resourcesCookie, nullptr);
    if (!resources)
        return;

    const int outputCount = xcb_randr_get_screen_resources_current_outputs_length(resources);
    const auto *outputs = xcb_randr_get_screen_resources_current_outputs(resources);
    for (int i = 0; i < outputCount; ++i) {
        const auto outputInfoCookie =
            xcb_randr_get_output_info(connection, outputs[i], resources->config_timestamp);
        auto *outputInfo =
            xcb_randr_get_output_info_reply(connection, outputInfoCookie, nullptr);
        if (!outputInfo)
            continue;

        const QString outputName = QString::fromUtf8(
            reinterpret_cast<const char *>(xcb_randr_get_output_info_name(outputInfo)),
            xcb_randr_get_output_info_name_length(outputInfo));
        if (outputName == primaryOutputName) {
            xcb_randr_set_output_primary(connection, screen->root, outputs[i]);
            xcb_flush(connection);
            free(outputInfo);
            break;
        }
        free(outputInfo);
    }
    free(resources);
#endif
}

const QList<std::shared_ptr<Session>> &SessionManager::sessions() const
{
    return m_sessions;
}

/**
 * Get the currently active session
 *
 * @returns weak_ptr to the active session
 */
std::weak_ptr<Session> SessionManager::activeSession() const
{
    return m_activeSession;
}

/**
 * Get the default session
 *
 * The default session is the session for the system user (TREELAND_SYSTEM_USER).
 * It manages the default Wayland socket and XWayland instance.
 *
 * @returns shared_ptr to the default session
 */
std::shared_ptr<Session> SessionManager::globalSession() const
{
    return sessionForUser(QStringLiteral(TREELAND_SYSTEM_USER));
}

/**
 * Check if the active session's WSocket is enabled
 *
 * @returns true if enabled, false otherwise
 */
bool SessionManager::activeSocketEnabled() const
{
    auto ptr = m_activeSession.lock();
    if (ptr && ptr->m_socket)
        return ptr->m_socket->isEnabled();
    return false;
}

/**
 * Set the active session's WSocket enabled state
 *
 * @param newEnabled New enabled state
 */
void SessionManager::setActiveSocketEnabled(bool newEnabled)
{
    auto ptr = m_activeSession.lock();
    if (ptr && ptr->m_socket) {
        const auto global = globalSession();
        ptr->m_socket->setEnabled(newEnabled, global ? global->socket() : nullptr);
    } else {
        qCWarning(lcTlCore) << "Can't set enabled for empty socket!";
    }
}

/**
 * Remove a session from the session list
 *
 * @param session The session to remove
 */
void SessionManager::removeSession(std::shared_ptr<Session> session)
{
    if (!session)
        return;

    if (m_activeSession.lock() == session) {
        const auto fallback = globalSession();
        if (fallback && fallback != session)
            updateActiveUserSession(fallback->username(), fallback->id());
        else
            m_activeSession.reset();
        session->workspace()->clearActivedSurface();
        WaylandServer::instance()->activateSurface(nullptr);
    }

    for (auto s : std::as_const(m_sessions)) {
        if (s.get() == session.get()) {
            m_sessions.removeOne(s);
            break;
        }
    }
}

/**
 * Ensure a session exists for the given username, creating it if necessary
 *
 * @param id An existing logind session ID
 * @param username Username to ensure session for
 * @returns Session for the given username, or nullptr on failure
 */
std::shared_ptr<Session> SessionManager::ensureSession(const QString &id, QString username)
{
    // WaylandServer lambda to create WSocket and WXWayland
    auto createWSocket = [this]() {
        // Create WSocket
        auto socket = new WSocket(true, nullptr);
        if (!socket->autoCreate()) {
            qCCritical(lcTlCore) << "Failed to create Wayland socket";
            delete socket;
            return static_cast<WSocket *>(nullptr);
        }
        // Connect signals
        connect(socket, &WSocket::fullServerNameChanged, this, [this] {
            if (m_activeSession.lock())
                Q_EMIT socketFileChanged();
        });
        // Add socket to server
        WaylandServer::instance()->addSocket(socket);
        return socket;
    };
#ifndef DISABLE_XWAYLAND
    auto createXWayland = [this](Session *owner, WSocket *socket) {
        // Create xwayland
        auto *xwayland = WaylandServer::instance()->createXWayland(owner->seatManager()->fallbackSeat());
        if (!xwayland) {
            qCCritical(lcTlCore) << "Failed to create XWayland instance";
            return static_cast<WXWayland *>(nullptr);
        }
        // Bind xwayland to socket
        xwayland->setOwnsSocket(socket);
        // Connect signals
        connect(xwayland, &WXWayland::ready, this, [this, xwayland] {
            Q_EMIT xwaylandAuthChanged();
            syncActiveSessionXWaylandPrimaryOutput();
            if (auto session = sessionForXWayland(xwayland)) {
                session->m_noTitlebarAtom =
                    internAtom(session->m_xwayland->xcbConnection(), _DEEPIN_NO_TITLEBAR, false);
                if (!session->m_noTitlebarAtom) {
                    qCWarning(lcTlInput) << "Failed to intern atom:" << _DEEPIN_NO_TITLEBAR;
                }
                session->m_settingManager = new SettingManager(session->m_xwayland->xcbConnection());
                session->m_settingManagerThread = new QThread();

                session->m_settingManager->moveToThread(session->m_settingManagerThread);

                const qreal scale = session->outputManager()->effectiveScale();
                const auto renderWindow = WaylandServer::instance()->window();
                connect(session->m_settingManagerThread,
                        &QThread::started,
                        this,
                        [this, settingManager = QPointer(session->m_settingManager),
                         scale,
                         renderWindow,
                         config = session->config()] {
                            if (!settingManager) {
                                qCDebug(lcTlXsettings)
                                    << "Skip initial XSettings sync: no SettingManager";
                                return;
                            }

                            const QString cursorTheme =
                                config ? config->cursorThemeName() : QString();
                            const qreal cursorSize = config ? config->cursorSize() : 24;
                            const QString imModule = config ? config->x11GtkIMModule() : QString();

                            const bool queued = QMetaObject::invokeMethod(
                                settingManager,
                                [settingManager, scale, cursorTheme, cursorSize, imModule]() {
                                    settingManager->setGlobalScale(scale);
                                    settingManager->setCursorTheme(cursorTheme);
                                    settingManager->setCursorSize(cursorSize);
                                    settingManager->setIMModule(imModule);
                                    settingManager->apply();
                                },
                                Qt::QueuedConnection);
                            if (!queued) {
                                qCWarning(lcTlXsettings)
                                    << "Failed to queue initial XSettings sync"
                                    << "cursorTheme:" << cursorTheme << "cursorSize:" << cursorSize
                                    << "imModule:" << imModule << "scale:" << scale;
                            }
                            QObject::connect(
                                renderWindow,
                                &WOutputRenderWindow::effectiveDevicePixelRatioChanged,
                                this,
                                &SessionManager::syncSessionScaleSettings,
                                Qt::UniqueConnection);
                        });
                connect(session->m_settingManagerThread, &QThread::finished, session->m_settingManagerThread, &QThread::deleteLater);
                session->m_settingManagerThread->start();
            }
        });
        return xwayland;
    };
#endif
    // Check if session already exists for user
    if (auto session = sessionForUser(username)) {
        // Ensure it has a socket and xwayland
        if (!session->m_socket) {
            auto *socket = createWSocket();
            if (!socket) {
                m_sessions.removeOne(session);
                return nullptr;
            }
            session->m_socket = socket;
        }
#ifndef DISABLE_XWAYLAND
        if (!session->m_xwayland) {
            auto *xwayland = createXWayland(session.get(), session->m_socket);
            if (!xwayland) {
                session->m_socket->close();
                m_sessions.removeOne(session);
                return nullptr;
            }

            session->m_xwayland = xwayland;
        }
#endif

        return session;
    }
    // Session does not exist, create new session with deleter
    auto passwd = getpwnam(username.toLocal8Bit().data());
    if (!passwd) {
        qCWarning(lcTlCore) << "Failed to get passwd entry for user:" << username;
        return nullptr;
    }
    auto session = std::make_shared<Session>();
    session->m_id = id;
    session->m_username = username;
    session->m_uid = passwd->pw_uid;
    session->m_config = DConfigManager::instance()->userConfig(username);
    auto *runtime = WaylandServer::instance();
    auto *workspace = runtime->shellHandler()->createSessionWorkspace(session->m_config);
    session->m_desktop = new DesktopEnvironment(session->m_config, workspace, runtime,
        [username](const QString &seatName) {
            return DConfigManager::instance()->userSeatConfig(username, seatName);
        },
        [owner = session.get()] { return owner->seatManager()->seats(); },
        [this](wlr_output *excluded) { return wallpaperLibrary(excluded); }, session.get());

    session->m_socket = createWSocket();
    if (!session->m_socket)
        return nullptr;

    session->m_scene = new SessionScene(session.get());
    session->desktop()->attachInputChannel(new SessionInputChannel(session.get(), runtime));
    session->m_seatManager = new SessionSeatManager(session.get());
    session->m_protocols = new SessionProtocols(session.get());
    session->m_prelaunch = new SessionPrelaunch(session.get(), runtime);
    auto *outputProtocol = new WOutputManagerV1;
    session->attachInterface(outputProtocol);
    auto *channel = new SessionOutputChannel(session.get(), runtime->outputHardware(), runtime->rootContainer());
    session->m_outputManager = new SessionOutputManager(DConfigManager::instance()->globalConfig(),
        [](const QString &outputId) {
            return DConfigManager::instance()->outputConfig(outputId);
        }, session->desktop()->surfaces(), channel, runtime->outputHardware(), runtime->backend(), outputProtocol, session->desktop());
    session->desktop()->attachOutputs(session->m_outputManager);
    WaylandServer::instance()->initializeSessionProtocols(session.get());

    // Protocol tests and headless runs do not launch the wallpaper producer.
    if (!m_wallpaperLauncher && !qEnvironmentVariableIsSet("TREELAND_DISABLE_WALLPAPER")) {
        m_wallpaperLauncher = new WallpaperLauncher(session->socket()->rootSocket());
        m_wallpaperLauncher->start();
    }

#ifndef DISABLE_XWAYLAND
    session->m_xwayland = createXWayland(session.get(), session->m_socket);
    if (!session->m_xwayland)
        return nullptr;
#endif

    session->m_socket->setEnabled(false, globalSession() ? globalSession()->socket() : nullptr);
    // Add session to list
    m_sessions.append(session);
    return session;
}


/**
 * Find the session for the given logind session id
 *
 * @param id Session ID to find session for
 * @returns Session for the given id, or nullptr if not found
 */
std::shared_ptr<Session> SessionManager::sessionForId(const QString &id) const
{
    for (const auto &session : std::as_const(m_sessions)) {
        if (session && session->m_id == id)
            return session;
    }
    return nullptr;
}

/**
 * Find the session for the given uid
 *
 * @param uid User ID to find session for
 * @returns Session for the given uid, or nullptr if not found
 */
std::shared_ptr<Session> SessionManager::sessionForUid(uid_t uid) const
{
    for (const auto &session : std::as_const(m_sessions)) {
        if (session && session->m_uid == uid)
            return session;
    }
    return nullptr;
}

/**
 * Find the session for the given username
 *
 * @param username Username to find session for
 * @returns Session for the given username, or nullptr if not found
 */
std::shared_ptr<Session> SessionManager::sessionForUser(const QString &username) const
{
    for (const auto &session : std::as_const(m_sessions)) {
        if (session && session->m_username == username)
            return session;
    }
    return nullptr;
}

#ifndef DISABLE_XWAYLAND
/**
 * Find the session for the given WXWayland
 *
 * @param xwayland WXWayland to find session for
 * @returns Session for the given xwayland, or nullptr if not found
 */
std::shared_ptr<Session> SessionManager::sessionForXWayland(WXWayland *xwayland) const
{
    for (const auto &session : std::as_const(m_sessions)) {
        if (session && session->m_xwayland == xwayland)
            return session;
    }
    return nullptr;
}
#endif

/**
 * Find the session for the given WSocket
 *
 * @param socket WSocket to find session for
 * @returns Session for the given socket, or nullptr if not found
 */
std::shared_ptr<Session> SessionManager::sessionForSocket(WSocket *socket) const
{
    for (const auto &session : std::as_const(m_sessions)) {
        if (session && session->m_socket == socket)
            return session;
    }
    return nullptr;
}

bool SessionManager::isDDEUserClient(WClient *client)
{
    // The dde sentinel session is created at startup, but defensive null
    // checks guard against it being absent (see issue #1464): dereferencing
    // a null globalSession() here previously caused a SIGSEGV.
    if (!client)
        return false;
    const auto global = globalSession();
    if (!global || !global->socket())
        return false;
    return client->socket() == global->socket();
}

void SessionManager::syncSessionScaleSettings()
{
#ifndef DISABLE_XWAYLAND
    for (const auto &session : std::as_const(m_sessions)) {
        if (!session || !session->m_settingManager)
            continue;
        const qreal scale = session->outputManager()->effectiveScale();
        const auto settings = QPointer(session->m_settingManager);
        QMetaObject::invokeMethod(settings, [settings, scale] {
            if (!settings)
                return;
            settings->setGlobalScale(scale);
            settings->apply();
        }, Qt::QueuedConnection);
    }
#endif
}

void SessionManager::syncActiveSessionXSettings()
{
#ifndef DISABLE_XWAYLAND
    const auto session = m_activeSession.lock();
    if (!session) {
        qCDebug(lcTlXsettings) << "Skip XSettings sync: no active session";
        return;
    }

    if (!session->m_settingManager) {
        qCDebug(lcTlXsettings)
            << "Skip XSettings sync: no SettingManager for session" << session->username();
        return;
    }

    const auto *config = session->config();
    if (!config) {
        qCWarning(lcTlXsettings) << "Cannot sync XSettings: user config is unavailable";
        return;
    }

    applySessionXSettings(session->m_settingManager,
                          config->cursorThemeName(),
                          config->cursorSize(),
                          config->x11GtkIMModule());
#endif
}

/**
 * Update the active session to the given uid, creating it if necessary.
 * This will update XWayland visibility and emit socketFileChanged if the
 * active session changed.
 *
 * @param username Username to set as active session
 */
void SessionManager::updateActiveUserSession(const QString &username, const QString &id)
{
    commitActiveUserSession(prepareActiveUserSession(username, id));
}

SessionManager::ActiveSessionUpdate SessionManager::prepareActiveUserSession(const QString &username,
                                                                             const QString &id)
{
    // Get previous active session
    auto previous = m_activeSession.lock();
    // Get new session for uid, creating if necessary
    auto session = ensureSession(id, username);
    if (!session) {
        qCWarning(lcTlInput) << "Failed to ensure session for user" << username;
        return {};
    }

    const bool activeSessionChanged = previous != session;
    if (activeSessionChanged) {
        auto *server = WaylandServer::instance();
        if (previous)
            previous->setActive(false);
        server->rootSurfaceContainer()->dismissAllPopups();
        for (auto *seat : server->seatManager()->seats()) {
            server->rootSurfaceContainer()->cancelMoveResizeForSeat(seat);
            server->activateSurface(nullptr, Qt::OtherFocusReason, seat);
        }
        // Publish the workspace through one path for all activation callers.
        m_activeSession = session;
        server->seatManager()->activateSessionSeats(session->seatManager());
        server->shellHandler()->setWorkspace(session->workspace());

        // Update socket enabled state before publishing activation notifications.
        if (previous && previous->m_socket) {
            const auto global = globalSession();
            previous->m_socket->setEnabled(false, global ? global->socket() : nullptr);
        }
        session->m_socket->setEnabled(true);
        session->setActive(true);
    }

    return { session, activeSessionChanged };
}

void SessionManager::commitActiveUserSession(const ActiveSessionUpdate &update)
{
    if (!update || m_activeSession.lock() != update.session)
        return;

    if (update.activeSessionChanged) {
        Q_EMIT socketFileChanged();
        // Notify session changed through DBus, treeland-sd will listen it to update envs
        Q_EMIT sessionChanged();
        syncActiveSessionXWaylandPrimaryOutput();
    }

    qCInfo(lcTlCore) << "Listening on:" << update.session->m_socket->fullServerName();
    syncActiveSessionXSettings();
    syncSessionScaleSettings();
    auto *server = WaylandServer::instance();
    if (update.activeSessionChanged && server->currentMode() == WaylandServer::CurrentMode::Normal)
        server->activateSurface(update.session->workspace()->current()->latestActiveSurface());
}

WallpaperManager::WallpaperLibrary SessionManager::wallpaperLibrary(wlr_output *excluded) const
{
    WallpaperManager::WallpaperLibrary result;
    for (const auto &owner : m_sessions) {
        const auto used = owner->wallpaperManager()->globalValidWallpaper(excluded);
        for (auto it = used.cbegin(); it != used.cend(); ++it)
            result.insert(it.key(), it.value());
    }
    return result;
}
