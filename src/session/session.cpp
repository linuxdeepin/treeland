// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "session.h"

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
#ifndef DISABLE_XWAYLAND
#include <wxwayland.h>
#endif

#include <pwd.h>
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

Session::~Session()
{
    qCDebug(lcTlCore) << "Deleting session for uid:" << m_uid << m_socket;
    Q_EMIT aboutToBeDestroyed();

#ifndef DISABLE_XWAYLAND
    if (m_settingManagerThread) {
        m_settingManagerThread->quit();
        m_settingManagerThread->wait(QDeadlineTimer(25000));
    }

    if (m_settingManager) {
        delete m_settingManager;
        m_settingManager = nullptr;
    }
    if (m_xwayland) {
        // if shellHandler is already destructed, wait for WServer to clean up the interface.
        if (auto *server = WaylandServer::instance())
            server->shellHandler()->removeXWayland(m_xwayland);
        m_xwayland = nullptr;
    }
#endif
    if (m_socket) {
        delete m_socket;
        m_socket = nullptr;
    }
}

const QString &Session::id() const
{
    return m_id;
}

uid_t Session::uid() const
{
    return m_uid;
}

const QString &Session::username() const
{
    return m_username;
}

WSocket *Session::socket() const
{
    return m_socket;
}

#ifndef DISABLE_XWAYLAND
WXWayland *Session::xwayland() const
{
    return m_xwayland;
}

quint32 Session::noTitlebarAtom() const
{
    return m_noTitlebarAtom;
}
#endif

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
 * The default session is the session for the "dde" user.
 * It manages the default Wayland socket and XWayland instance.
 *
 * @returns shared_ptr to the default session
 */
std::shared_ptr<Session> SessionManager::globalSession() const
{
    return sessionForUser("dde");
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
        m_activeSession.reset();
        WaylandServer::instance()->workspace()->clearActivedSurface();
        WaylandServer::instance()->activateSurface(nullptr);
        // No session is active anymore, so nothing may see the screen from now
        // on (the pending screen captures are paused by this notification).
        Q_EMIT activeSessionRemoved();
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
    auto createXWayland = [this](WSocket *socket) {
        // Create xwayland
        auto *xwayland = WaylandServer::instance()->createXWayland();
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

                const qreal scale = WaylandServer::instance()->rootSurfaceContainer()->window()->effectiveDevicePixelRatio();
                const auto renderWindow = WaylandServer::instance()->window();
                connect(session->m_settingManagerThread,
                        &QThread::started,
                        this,
                        [settingManager = QPointer(session->m_settingManager),
                         scale,
                         renderWindow] {
                            if (!settingManager) {
                                qCDebug(lcTlXsettings)
                                    << "Skip initial XSettings sync: no SettingManager";
                                return;
                            }

                            const auto *config = WaylandServer::instance()->config();
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
                                settingManager,
                                [settingManager](qreal dpr) {
                                    settingManager->setGlobalScale(dpr);
                                    settingManager->apply();
                                },
                                Qt::QueuedConnection);
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
            auto *xwayland = createXWayland(session->m_socket);
            if (!xwayland) {
                delete session->m_socket;
                session->m_socket = nullptr;
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

    session->m_socket = createWSocket();
    if (!session->m_socket)
        return nullptr;

    // Protocol tests and headless runs have no use for the wallpaper producer.
    if (!m_wallpaperLauncher && !qEnvironmentVariableIsSet("TREELAND_DISABLE_WALLPAPER")) {
        m_wallpaperLauncher = new WallpaperLauncher(session->socket()->rootSocket());
        m_wallpaperLauncher->start();
    }

#ifndef DISABLE_XWAYLAND
    session->m_xwayland = createXWayland(session->m_socket);
    if (!session->m_xwayland)
        return nullptr;
#endif

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

/**
 * Whether the client belongs to the active session
 *
 * Clients created through wp_security_context_v1 live on a child socket, so
 * the comparison is done on root sockets.
 *
 * @param client Client to check
 * @returns true if the client belongs to the active session
 */
bool SessionManager::isActiveSessionClient(WClient *client) const
{
    const auto session = m_activeSession.lock();
    if (!client || !client->socket() || !session || !session->socket())
        return false;
    return client->socket()->rootSocket() == session->socket()->rootSocket();
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

    auto *server = WaylandServer::instance();
    Q_ASSERT(server);

    const auto *config = server->config();
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
        // Update active session
        m_activeSession = session;
        // Clear activated surface
        // TODO: Each Wayland socket's active surface needs to be cleaned up individually.
        WaylandServer::instance()->activateSurface(nullptr);
        // Update socket enabled state before publishing activation notifications.
        if (previous && previous->m_socket) {
            const auto global = globalSession();
            previous->m_socket->setEnabled(false, global ? global->socket() : nullptr);
        }
        session->m_socket->setEnabled(true);
    }

    return { session, activeSessionChanged };
}

void SessionManager::commitActiveUserSession(const ActiveSessionUpdate &update)
{
    if (!update)
        return;

    if (update.activeSessionChanged) {
        Q_EMIT socketFileChanged();
        // Notify session changed through DBus, treeland-sd will listen it to update envs
        Q_EMIT sessionChanged();
        syncActiveSessionXWaylandPrimaryOutput();
    }

    qCInfo(lcTlCore) << "Listening on:" << update.session->m_socket->fullServerName();
    syncActiveSessionXSettings();
}
