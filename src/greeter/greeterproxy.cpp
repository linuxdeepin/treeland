// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "greeterproxy.h"

// Treeland
#include "common/treelandlogging.h"
#include "core/lockscreen.h"
#include "greeter/sessionmodel.h"
#include "greeter/usermodel.h"
#include "modules/ddm/ddminterfacev2.h"
#include "seat/helper.h"
#include "session/session.h"

// DDM
#include <Login1Manager.h>
#include <Login1Session.h>
#include <Messages.h>

// Qt
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusReply>
#include <QGuiApplication>
#include <QScopeGuard>
#include <QVariantMap>

// System
#include <security/pam_appl.h>
#include <systemd/sd-login.h>

using namespace DDM;

/////////////////////
// Local Functions //
/////////////////////

static bool localValidation(const QString &user, const QString &password)
{
    auto utf8Password = password.toUtf8();
    struct pam_conv conv = {
        []([[maybe_unused]] int num_msg,
           [[maybe_unused]] const struct pam_message **msg,
           struct pam_response **resp,
           void *appdata_ptr) {
            // pam uses free, we must malloc
            auto *reply = static_cast<pam_response *>(malloc(sizeof(pam_response)));
            reply->resp = strdup(static_cast<const char *>(appdata_ptr)); // Send password to PAM
            reply->resp_retcode = 0;
            *resp = reply;
            return PAM_SUCCESS;
        },
        static_cast<void *>(utf8Password.data()),
    };

    pam_handle_t *pamh = nullptr;

    int retval = pam_start("login", user.toUtf8().data(), &conv, &pamh);
    if (retval != PAM_SUCCESS) {
        return false;
    }

    retval = pam_authenticate(pamh, 0);
    pam_end(pamh, retval);

    return retval == PAM_SUCCESS;
}

static inline UserModel *userModel()
{
    return Helper::instance()->userModel();
}

static inline SessionModel *sessionModel()
{
    return Helper::instance()->sessionModel();
}

/////////////////////////////////
// Constructor & Deconstructor //
/////////////////////////////////

GreeterProxy::GreeterProxy(QObject *parent)
    : QObject(parent)
{
    auto conn = QDBusConnection::systemBus();
    conn.connect(Logind::serviceName(),
                 Logind::managerPath(),
                 Logind::managerIfaceName(),
                 "SessionNew",
                 this,
                 SLOT(onSessionNew(QString, QDBusObjectPath)));
    conn.connect(Logind::serviceName(),
                 Logind::managerPath(),
                 Logind::managerIfaceName(),
                 "SessionRemoved",
                 this,
                 SLOT(onSessionRemoved(QString, QDBusObjectPath)));
}

GreeterProxy::~GreeterProxy() { }

void GreeterProxy::connectDDM(DDMInterfaceV2 *interface)
{
    m_ddmInterface = interface;
    connect(interface, &DDMInterfaceV2::capabilities, this, &GreeterProxy::capabilities);
    connect(interface, &DDMInterfaceV2::userLoggedIn, this, &GreeterProxy::userLoggedIn);
    connect(interface,
            &DDMInterfaceV2::authenticationFailed,
            this,
            &GreeterProxy::authenticationFailed);
}

////////////////////////
// Properties setters //
////////////////////////

void GreeterProxy::setShowShutdownView(bool show)
{
    if (m_showShutdownView != show) {
        m_showShutdownView = show;
        Q_EMIT showShutdownViewChanged(show);
    }
}

void GreeterProxy::setLock(bool isLocked)
{
    if (isLocked && !m_isLocked) {
        m_isLocked = true;
        if (m_lockScreen && !m_lockScreen->isVisible())
            m_lockScreen->lock();
        Q_EMIT lockChanged(true);
    } else if (!isLocked && m_isLocked) {
        m_failedAttempts = 0;
        Q_EMIT failedAttemptsChanged(0);
        m_isLocked = false;
        if (m_lockScreen && m_lockScreen->isVisible())
            Q_EMIT m_lockScreen->unlock();
        Q_EMIT lockChanged(false);
    }
    if (m_showShutdownView)
        setShowShutdownView(false);
}

///////////
// Slots //
///////////

void GreeterProxy::powerOff()
{
    if (m_ddmInterface && m_ddmInterface->isValid())
        m_ddmInterface->powerOff();
}

void GreeterProxy::reboot()
{
    if (m_ddmInterface && m_ddmInterface->isValid())
        m_ddmInterface->reboot();
}

void GreeterProxy::suspend()
{
    if (m_ddmInterface && m_ddmInterface->isValid())
        m_ddmInterface->suspend();
}

void GreeterProxy::hibernate()
{
    if (m_ddmInterface && m_ddmInterface->isValid())
        m_ddmInterface->hibernate();
}

void GreeterProxy::hybridSleep()
{
    if (m_ddmInterface && m_ddmInterface->isValid())
        m_ddmInterface->hybridSleep();
}

void GreeterProxy::login(const QString &user, const QString &password, const int sessionIndex)
{
    if (m_ddmInterface && m_ddmInterface->isValid()) {
        // get model index
        QModelIndex index = sessionModel()->index(sessionIndex, 0);
        // send command to the daemon
        DDM::Session::Type sessionType = static_cast<DDM::Session::Type>(
            sessionModel()->data(index, SessionModel::TypeRole).toInt());
        QString sessionFile = sessionModel()->data(index, SessionModel::FileRole).toString();
        qCInfo(treelandGreeter) << "Logging user" << user << "in with" << sessionType << "session"
                                << sessionFile;
        m_ddmInterface->login(user, password, sessionType, sessionFile);
    } else {
        qCInfo(treelandGreeter) << "DDM is not valid. Local password check.";
        if (localValidation(user, password)) {
            setLock(false);
        } else {
            Q_EMIT failedAttemptsChanged(++m_failedAttempts);
        }
    }
}

void GreeterProxy::unlock(const QString &user, const QString &password)
{
    if (m_ddmInterface && m_ddmInterface->isValid()) {
        auto session = Helper::instance()->sessionManager()->sessionForUser(user);
        if (session) {
            qCInfo(treelandGreeter) << "Unlocking session" << session->id() << "for user" << user;
            m_ddmInterface->unlock(session->id(), password);
        } else {
            qCWarning(treelandGreeter) << "Trying to unlock session for user" << user
                                       << "but no session found.";
            // [TODO] Further actions
        }
    } else {
        qCInfo(treelandGreeter) << "DDM is not valid. Local password check.";
        if (localValidation(user, password)) {
            setLock(false);
        } else {
            Q_EMIT failedAttemptsChanged(++m_failedAttempts);
        }
    }
}

void GreeterProxy::logout()
{
    if (m_ddmInterface && m_ddmInterface->isValid()) {
        auto session = Helper::instance()->sessionManager()->activeSession().lock();
        if (session) {
            qCInfo(treelandGreeter) << "Logging user" << session->username() << "out with session id"
                                    << session->id();
            m_ddmInterface->logout(session->id());
        } else {
            qCWarning(treelandGreeter)
                << "Trying to logout when no active session, show lockscreen directly.";
            setLock(true);
        }
    } else {
        qCInfo(treelandGreeter)
            << "Trying to logout when DDM is not available, show lockscreen directly.";
        setLock(true);
    }
}

void GreeterProxy::lock()
{
    auto session = Helper::instance()->sessionManager()->activeSession().lock();
    if (!session || session->username() == "dde") {
        qCInfo(treelandGreeter)
            << "Trying to lock when no user session active, show lockscreen directly.";
        setLock(true);
    } else if (!m_ddmInterface || !m_ddmInterface->isValid()) {
        qCInfo(treelandGreeter)
            << "Trying to lock when DDM is not available, show lockscreen directly.";
        setLock(true);
    } else {
        qCInfo(treelandGreeter) << "Locking user" << session->username() << "with session id"
                                << session->id();
        m_ddmInterface->lock(session->id());
    }
}

//////////////////////
// Signals from DDM //
//////////////////////

void GreeterProxy::capabilities(uint32_t capabilities)
{
    // parse capabilities
    m_canPowerOff = capabilities & Capability::PowerOff;
    m_canReboot = capabilities & Capability::Reboot;
    m_canSuspend = capabilities & Capability::Suspend;
    m_canHibernate = capabilities & Capability::Hibernate;
    m_canHybridSleep = capabilities & Capability::HybridSleep;

    // Q_EMIT signals
    Q_EMIT canPowerOffChanged(m_canPowerOff);
    Q_EMIT canRebootChanged(m_canReboot);
    Q_EMIT canSuspendChanged(m_canSuspend);
    Q_EMIT canHibernateChanged(m_canHibernate);
    Q_EMIT canHybridSleepChanged(m_canHybridSleep);
}

void GreeterProxy::userLoggedIn(const QString &username, const QString &session)
{
    // This will happen after a crash recovery of treeland
    qCInfo(treelandGreeter) << "User " << username << " is already logged in with session"
                            << session;
    auto userPtr = userModel()->getUser(username);
    if (userPtr) {
        userModel()->updateUserLoginState(username, true);
        Q_EMIT userModel()->userLoggedIn(username, session);
        QThreadPool::globalInstance()->start([this, session] {
            // Connect to Lock/Unlock signals
            auto conn = QDBusConnection::systemBus();
            OrgFreedesktopLogin1ManagerInterface manager(Logind::serviceName(),
                                                         Logind::managerPath(),
                                                         conn);
            auto reply = manager.GetSession(session);
            reply.waitForFinished();
            if (!reply.isValid()) {
                qCWarning(treelandGreeter) << "Failed to get session path for session" << session
                                           << ", error:" << reply.error().message();
                return;
            }
            auto path = reply.value();
            conn.connect(Logind::serviceName(),
                         path.path(),
                         Logind::sessionIfaceName(),
                         "Lock",
                         this,
                         SLOT(onSessionLock()));
            conn.connect(Logind::serviceName(),
                         path.path(),
                         Logind::sessionIfaceName(),
                         "Unlock",
                         this,
                         SLOT(onSessionUnlock()));
        });
    } else {
        qCWarning(treelandGreeter) << "User " << username << " logged in but not found";
    }
}

void GreeterProxy::authenticationFailed(uint32_t error)
{
    qCDebug(treelandGreeter) << "Message received from DDM: Authentication Failed:"
                             << DDMInterfaceV2::authErrorToString(error);
    Q_EMIT failedAttemptsChanged(++m_failedAttempts);
}

//////////////////////////////
// Logind session listeners //
//////////////////////////////

void GreeterProxy::onSessionNew(const QString &id, [[maybe_unused]] const QDBusObjectPath &path)
{
    QByteArray sessionBa = id.toLocal8Bit();
    const char *session = sessionBa.constData();
    char *username = nullptr;
    char *service = nullptr;
    auto guard = qScopeGuard([&]() {
        if (username)
            free(username);
        if (service)
            free(service);
    });
    if (sd_session_get_username(session, &username) < 0) {
        qCWarning(treelandGreeter) << "sd_session_get_username() failed for session id:" << id;
        return;
    }
    if (sd_session_get_service(session, &service) < 0) {
        qCWarning(treelandGreeter) << "sd_session_get_service() failed for session id:" << id;
        return;
    }

    if (strcmp(service, "ddm") == 0) {
        QString user = QString::fromLocal8Bit(username);
        qCInfo(treelandGreeter) << "New session added: id=" << id << ", user=" << user;
        userModel()->updateUserLoginState(user, true);
        // userLoggedIn signal is connected with Helper::updateActiveUserSession
        Q_EMIT userModel()->userLoggedIn(user, id);

        // Connect to Lock/Unlock signals
        auto conn = QDBusConnection::systemBus();
        conn.connect(Logind::serviceName(),
                     path.path(),
                     Logind::sessionIfaceName(),
                     "Lock",
                     this,
                     SLOT(onSessionLock()));
        conn.connect(Logind::serviceName(),
                     path.path(),
                     Logind::sessionIfaceName(),
                     "Unlock",
                     this,
                     SLOT(onSessionUnlock()));

        if (userModel()->currentUserName() == user)
            setLock(false);
        if (!m_hasActiveSession) {
            m_hasActiveSession = true;
            Q_EMIT hasActiveSessionChanged(true);
        }
    }
}

void GreeterProxy::onSessionRemoved(const QString &id, [[maybe_unused]] const QDBusObjectPath &path)
{
    // Disconnect from Lock/Unlock signals, if any
    auto conn = QDBusConnection::systemBus();
    conn.disconnect(Logind::serviceName(),
                    path.path(),
                    Logind::sessionIfaceName(),
                    "Lock",
                    this,
                    SLOT(onSessionLock()));
    conn.disconnect(Logind::serviceName(),
                    path.path(),
                    Logind::sessionIfaceName(),
                    "Unlock",
                    this,
                    SLOT(onSessionUnlock()));

    auto session = Helper::instance()->sessionManager()->sessionForId(id);
    if (session) {
        QString username = session->username();
        qCInfo(treelandGreeter) << "Session removed: id=" << id << ", user=" << username;
        if (Helper::instance()->sessionManager()->activeSession().lock() == session)
            setLock(true);
        userModel()->updateUserLoginState(username, false);
        Helper::instance()->sessionManager()->removeSession(session);
    }

    if (m_hasActiveSession && Helper::instance()->sessionManager()->sessions().isEmpty()) {
        m_hasActiveSession = false;
        Q_EMIT hasActiveSessionChanged(false);
    }
}

void GreeterProxy::onSessionLock()
{
    const QString path = message().path();
    QThreadPool::globalInstance()->start([this, path] {
        OrgFreedesktopLogin1SessionInterface session("org.freedesktop.login1",
                                                     path,
                                                     QDBusConnection::systemBus());
        QString id = session.id();
        qCInfo(treelandGreeter) << "Lock signal received for session id:" << id;
        auto activeSession = Helper::instance()->sessionManager()->activeSession().lock();
        if (!activeSession)
            qCWarning(treelandGreeter)
                << "Lock signal received for non-exist session id:" << id << ", ignore.";
        else if (activeSession->id() != id)
            qCWarning(treelandGreeter)
                << "Lock signal received for non-active session id:" << id << ", ignore.";
        else
            QMetaObject::invokeMethod(this, [this] {
                setLock(true);
            });
    });
}

void GreeterProxy::onSessionUnlock()
{
    const QString path = message().path();
    QThreadPool::globalInstance()->start([this, path] {
        OrgFreedesktopLogin1SessionInterface session("org.freedesktop.login1",
                                                     path,
                                                     QDBusConnection::systemBus());
        QString id = session.id();
        const QString username = session.name();
        qCInfo(treelandGreeter) << "Unlock signal received for session id:" << id;
        auto activeSession = Helper::instance()->sessionManager()->activeSession().lock();
        if (!activeSession) {
            qCWarning(treelandGreeter)
                << "Unlock signal received for non-exist session id:" << id << ", ignore.";
        } else if (activeSession->id() != id) {
            qCWarning(treelandGreeter)
                << "Unlock signal received for non-active session id:" << id << ", lock it back.";
            QMetaObject::invokeMethod(this, [this, id] {
                if (m_ddmInterface && m_ddmInterface->isValid()) {
                    m_ddmInterface->lock(id);
                } else {
                    qCInfo(treelandGreeter) << "Trying to lock when DDM is not available, show lockscreen directly." << id;
                    setLock(true);
                }
            });
        } else {
            QMetaObject::invokeMethod(this, [this] {
                setLock(false);
            });
        }
    });
}
