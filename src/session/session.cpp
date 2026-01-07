// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "session.h"

#include "common/treelandlogging.h"
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "seat/helper.h"
#include "workspace/workspace.h"
#include "xsettings/settingmanager.h"

#include <woutputrenderwindow.h>
#include <wsocket.h>
#include <wxwayland.h>

#include <pwd.h>

#define _DEEPIN_NO_TITLEBAR "_DEEPIN_NO_TITLEBAR"

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

Session::~Session()
{
    qCDebug(treelandCore) << "Deleting session for uid:" << m_uid << m_socket;
    Q_EMIT aboutToBeDestroyed();

    if (m_settingManagerThread) {
        m_settingManagerThread->quit();
        m_settingManagerThread->wait(QDeadlineTimer(25000));
    }

    if (m_settingManager) {
        delete m_settingManager;
        m_settingManager = nullptr;
    }
    if (m_xwayland)
        Helper::instance()->shellHandler()->removeXWayland(m_xwayland);
    if (m_socket)
        delete m_socket;
}

int Session::id() const
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

WXWayland *Session::xwayland() const
{
    return m_xwayland;
}

quint32 Session::noTitlebarAtom() const
{
    return m_noTitlebarAtom;
}

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
}

SessionManager::~SessionManager()
{
    m_sessions.clear();
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
    if (ptr && ptr->m_socket)
        ptr->m_socket->setEnabled(newEnabled);
    else
        qCWarning(treelandCore) << "Can't set enabled for empty socket!";
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
        Helper::instance()->workspace()->clearActivedSurface();
        Helper::instance()->activateSurface(nullptr);
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
std::shared_ptr<Session> SessionManager::ensureSession(int id, QString username)
{
    // Helper lambda to create WSocket and WXWayland
    auto createWSocket = [this]() {
        // Create WSocket
        auto socket = new WSocket(true, nullptr);
        if (!socket->autoCreate()) {
            qCCritical(treelandCore) << "Failed to create Wayland socket";
            delete socket;
            return static_cast<WSocket *>(nullptr);
        }
        // Connect signals
        connect(socket, &WSocket::fullServerNameChanged, this, [this] {
            if (m_activeSession.lock())
                Q_EMIT socketFileChanged();
        });
        // Add socket to server
        Helper::instance()->addSocket(socket);
        return socket;
    };
    auto createXWayland = [this](WSocket *socket) {
        // Create xwayland
        auto *xwayland = Helper::instance()->createXWayland();
        if (!xwayland) {
            qCCritical(treelandCore) << "Failed to create XWayland instance";
            return static_cast<WXWayland *>(nullptr);
        }
        // Bind xwayland to socket
        xwayland->setOwnsSocket(socket);
        // Connect signals
        connect(xwayland, &WXWayland::ready, this, [this, xwayland] {
            if (auto session = sessionForXWayland(xwayland)) {
                session->m_noTitlebarAtom =
                    internAtom(session->m_xwayland->xcbConnection(), _DEEPIN_NO_TITLEBAR, false);
                if (!session->m_noTitlebarAtom) {
                    qCWarning(treelandInput) << "Failed to intern atom:" << _DEEPIN_NO_TITLEBAR;
                }
                session->m_settingManager = new SettingManager(session->m_xwayland->xcbConnection());
                session->m_settingManagerThread = new QThread();

                session->m_settingManager->moveToThread(session->m_settingManagerThread);

                const qreal scale = Helper::instance()->rootSurfaceContainer()->window()->effectiveDevicePixelRatio();
                const auto renderWindow = Helper::instance()->window();
                connect(session->m_settingManagerThread, &QThread::started,
                        this,
                        [settingManager = QPointer(session->m_settingManager),
                         scale, renderWindow] {
                            QMetaObject::invokeMethod(
                                settingManager,
                                [settingManager, scale]() {
                                    settingManager->setGlobalScale(scale);
                                    settingManager->apply();
                                },
                                Qt::QueuedConnection);
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

        return session;
    }
    // Session does not exist, create new session with deleter
    auto passwd = getpwnam(username.toLocal8Bit().data());
    if (!passwd) {
        qCWarning(treelandCore) << "Failed to get passwd entry for user:" << username;
        return nullptr;
    }
    auto session = std::make_shared<Session>();
    session->m_id = id;
    session->m_username = username;
    session->m_uid = passwd->pw_uid;

    session->m_socket = createWSocket();
    if (!session->m_socket)
        return nullptr;

    session->m_xwayland = createXWayland(session->m_socket);
    if (!session->m_xwayland)
        return nullptr;

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
std::shared_ptr<Session> SessionManager::sessionForId(int id) const
{
    for (auto session : m_sessions) {
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
    for (auto session : m_sessions) {
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
    for (auto session : m_sessions) {
        if (session && session->m_username == username)
            return session;
    }
    return nullptr;
}

/**
 * Find the session for the given WXWayland
 *
 * @param xwayland WXWayland to find session for
 * @returns Session for the given xwayland, or nullptr if not found
 */
std::shared_ptr<Session> SessionManager::sessionForXWayland(WXWayland *xwayland) const
{
    for (auto session : m_sessions) {
        if (session && session->m_xwayland == xwayland)
            return session;
    }
    return nullptr;
}

/**
 * Find the session for the given WSocket
 *
 * @param socket WSocket to find session for
 * @returns Session for the given socket, or nullptr if not found
 */
std::shared_ptr<Session> SessionManager::sessionForSocket(WSocket *socket) const
{
    for (auto session : m_sessions) {
        if (session && session->m_socket == socket)
            return session;
    }
    return nullptr;
}

/**
 * Update the active session to the given uid, creating it if necessary.
 * This will update XWayland visibility and emit socketFileChanged if the
 * active session changed.
 *
 * @param username Username to set as active session
 */
void SessionManager::updateActiveUserSession(const QString &username, int id)
{
    // Get previous active session
    auto previous = m_activeSession.lock();
    // Get new session for uid, creating if necessary
    auto session = ensureSession(id, username);
    if (!session) {
        qCWarning(treelandInput) << "Failed to ensure session for user" << username;
        return;
    }
    if (previous != session) {
        // Update active session
        m_activeSession = session;
        // Clear activated surface
        // TODO: Each Wayland socket's active surface needs to be cleaned up individually.
        Helper::instance()->activateSurface(nullptr);
        // Emit signal and update socket enabled state
        if (previous && previous->m_socket)
            previous->m_socket->setEnabled(false);
        session->m_socket->setEnabled(true);
        Q_EMIT socketFileChanged();
        // Notify session changed through DBus, treeland-sd will listen it to update envs
        Q_EMIT sessionChanged();
    }
    qCInfo(treelandCore) << "Listening on:" << session->m_socket->fullServerName();
}
