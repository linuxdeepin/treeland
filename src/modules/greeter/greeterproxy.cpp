// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "greeterproxy.h"

// Treeland
#include "common/treelandlogging.h"
#include "modules/greeter/lockscreen.h"
#include "modules/greeter/sessionmodel.h"
#include "modules/greeter/usermodel.h"
#include "seat/helper.h"
#include "session/session.h"

// DDM
#include <rep_ddmremote_replica.h>

// Qt
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQuickItem>
#include <QRemoteObjectNode>
#include <QRemoteObjectPendingCallWatcher>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

// Waylib
#include <woutputrenderwindow.h>

// System
#include <security/pam_appl.h>

#include <pwd.h>

namespace {
constexpr auto defaultDdmRemoteUrl = "local:org.deepin.dde.ddm.qro";
constexpr auto ddmRemoteUrlEnv = "TREELAND_DDM_REMOTE_URL";
constexpr auto dummyRefreshMessage = "__dummyddm_refresh_state__";
}

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
}

GreeterProxy::~GreeterProxy() { }

////////////////////////
// Properties setters //
////////////////////////

void GreeterProxy::setShowShutdownView(bool show)
{
    if (show)
        updatePowerCapabilities();
    if (m_showShutdownView != show) {
        m_showShutdownView = show;
        Q_EMIT showShutdownViewChanged(show);
    }
}

bool GreeterProxy::isLocked() const
{
    return m_lockScreen && m_lockScreen->isLocked();
}

void GreeterProxy::setLockScreen(LockScreen *lockScreen)
{
    if (m_lockScreen == lockScreen)
        return;
    const bool wasLocked = isLocked();

    if (m_lockScreenVisibleConnection) {
        disconnect(m_lockScreenVisibleConnection);
        m_lockScreenVisibleConnection = { };
    }

    m_lockScreen = lockScreen;
    if (m_lockScreen) {
        m_lockScreenVisibleConnection =
            connect(m_lockScreen, &QQuickItem::visibleChanged, this, [this] {
                Q_EMIT lockChanged(isLocked());
            });
    }

    const bool locked = isLocked();
    if (wasLocked != locked)
        Q_EMIT lockChanged(locked);
}

void GreeterProxy::setLock(bool isLocked)
{
    if (isLocked)
        updatePowerCapabilities();
    if (isLocked) {
        if (m_lockScreen && !m_lockScreen->isLocked())
            m_lockScreen->lock();
    } else {
        m_failedAttempts = 0;
        Q_EMIT failedAttemptsChanged(0);
        if (m_lockScreen && m_lockScreen->isLocked())
            Q_EMIT m_lockScreen->unlock();
    }
    if (m_showShutdownView)
        setShowShutdownView(false);
}

///////////
// Slots //
///////////

void GreeterProxy::powerOff()
{
    if (isConnected())
        watchRemoteCall("powerOff", m_remoteReplica->powerOff());
}

void GreeterProxy::reboot()
{
    if (isConnected())
        watchRemoteCall("reboot", m_remoteReplica->reboot());
}

void GreeterProxy::suspend()
{
    if (isConnected())
        watchRemoteCall("suspend", m_remoteReplica->suspend());
}

void GreeterProxy::hibernate()
{
    if (isConnected())
        watchRemoteCall("hibernate", m_remoteReplica->hibernate());
}

void GreeterProxy::hybridSleep()
{
    if (isConnected())
        watchRemoteCall("hybridSleep", m_remoteReplica->hybridSleep());
}

void GreeterProxy::login(const QString &user, const QString &password, const int sessionIndex)
{
    auto userInfo = userModel()->getUser(user);
    if (!isConnected() || !userInfo) {
        qCDebug(treelandGreeter) << "Socket is not valid or user not found. Local password check.";
        if (localValidation(user, password)) {
            setLock(false);
        } else {
            Q_EMIT failedAttemptsChanged(++m_failedAttempts);
        }
        return;
    }

    if (userInfo->loggedIn()) {
        qCInfo(treelandGreeter) << "Unlocking user" << user;
        if (localValidation(user, password)) {
            setLock(false);
        } else {
            Q_EMIT failedAttemptsChanged(++m_failedAttempts);
        }
        return;
    }

    // get model index
    QModelIndex index = sessionModel()->index(sessionIndex, 0);

    // send command to the daemon
    const int type = sessionModel()->data(index, SessionModel::TypeRole).toInt();
    const QString name = sessionModel()->data(index, SessionModel::FileRole).toString();
    qCInfo(treelandGreeter) << "Logging user" << user << "in with" << type << "session" << name;
    watchRemoteCall("login", m_remoteReplica->login(user, password, type, name));
}

void GreeterProxy::logout()
{
    auto session = Helper::instance()->sessionManager()->activeSession().lock();
    if (!session) {
        qCWarning(treelandGreeter) << "Trying to logout when no user session active.";
        return;
    }

    qCInfo(treelandGreeter) << "Logging user" << session->username() << "out with session id"
                            << session->id();
    if (isConnected())
        watchRemoteCall("logout", m_remoteReplica->logout(session->id()));
}

void GreeterProxy::updatePowerCapabilities()
{
    if (!isConnected())
        return;
    watchPowerCapability("canPowerOff",
                         m_remoteReplica->canPowerOff(),
                         &GreeterProxy::setCanPowerOff);
    watchPowerCapability("canReboot", m_remoteReplica->canReboot(), &GreeterProxy::setCanReboot);
    watchPowerCapability("canSuspend", m_remoteReplica->canSuspend(), &GreeterProxy::setCanSuspend);
    watchPowerCapability("canHibernate",
                         m_remoteReplica->canHibernate(),
                         &GreeterProxy::setCanHibernate);
    watchPowerCapability("canHybridSleep",
                         m_remoteReplica->canHybridSleep(),
                         &GreeterProxy::setCanHybridSleep);
}

void GreeterProxy::lock()
{
    setLock(true);
}

////////////////////////////
// DDM session listeners //
////////////////////////////

void GreeterProxy::addUserSession(const QString &user, int sessionId)
{
    qCInfo(treelandGreeter) << "User session added: id=" << sessionId << ", user=" << user;
    auto userPtr = userModel()->getUser(user);
    if (!userPtr) {
        qCWarning(treelandGreeter) << "User" << user << "logged in but not found";
        return;
    }

    userModel()->updateUserLoginState(user, true);
    Q_EMIT userModel()->userLoggedIn(user, sessionId);

    if (userModel()->currentUserName() == user)
        setLock(false);
    if (!m_hasActiveSession) {
        m_hasActiveSession = true;
        Q_EMIT hasActiveSessionChanged(true);
    }
}

void GreeterProxy::removeUserSession(const QString &user, int sessionId)
{
    qCInfo(treelandGreeter) << "User session removed: id=" << sessionId << ", user=" << user;
    auto session = Helper::instance()->sessionManager()->sessionForId(sessionId);
    if (session) {
        if (Helper::instance()->sessionManager()->activeSession().lock() == session)
            setLock(true);
        Helper::instance()->sessionManager()->removeSession(session);
    }
    userModel()->updateUserLoginState(user, false);

    if (m_hasActiveSession && Helper::instance()->sessionManager()->sessions().isEmpty()) {
        m_hasActiveSession = false;
        Q_EMIT hasActiveSessionChanged(false);
    }
}

///////////////////////
// DDM Communication //
///////////////////////

QString GreeterProxy::hostName() const
{
    return m_remoteReplica ? m_remoteReplica->hostName() : QString();
}

bool GreeterProxy::canPowerOff() const
{
    return m_canPowerOff;
}

bool GreeterProxy::canReboot() const
{
    return m_canReboot;
}

bool GreeterProxy::canSuspend() const
{
    return m_canSuspend;
}

bool GreeterProxy::canHibernate() const
{
    return m_canHibernate;
}

bool GreeterProxy::canHybridSleep() const
{
    return m_canHybridSleep;
}

bool GreeterProxy::isConnected() const
{
    return m_remoteReplica && m_remoteReplica->state() == QRemoteObjectReplica::Valid;
}

void GreeterProxy::setDDMConnectionEnabled(bool enabled)
{
    if (m_ddmConnectionEnabled == enabled)
        return;

    m_ddmConnectionEnabled = enabled;
    if (!m_ddmConnectionEnabled)
        resetRemote();
}

QUrl GreeterProxy::configuredDdmRemoteUrl()
{
    const auto env = qEnvironmentVariable(ddmRemoteUrlEnv);
    return env.isEmpty() ? QUrl(QString::fromLatin1(defaultDdmRemoteUrl))
                         : QUrl(env);
}

void GreeterProxy::connectToDDM()
{
    if (!m_ddmConnectionEnabled) {
        qCWarning(treelandGreeter) << "Skip DDM connection before Treeland remote object is ready";
        return;
    }

    resetRemote();
    m_remoteNode = std::make_unique<QRemoteObjectNode>();
    const QUrl remoteUrl = configuredDdmRemoteUrl();
    qCInfo(treelandGreeter) << "Connecting to DDM remote node:" << remoteUrl;
    if (!m_remoteNode->connectToNode(remoteUrl)) {
        qCCritical(treelandGreeter) << "Failed to connect to DDM remote node:" << remoteUrl;
        resetRemote();
        QTimer::singleShot(1000, this, &GreeterProxy::connectToDDM);
        return;
    }

    m_remoteReplica.reset(m_remoteNode->acquire<DDMRemoteReplica>());
    connect(m_remoteReplica.get(),
            &DDMRemoteReplica::hostNameChanged,
            this,
            &GreeterProxy::hostNameChanged);
    connect(m_remoteReplica.get(),
            &DDMRemoteReplica::informationMessage,
            this,
            [this](const QString &message) {
                if (message == QString::fromLatin1(dummyRefreshMessage)) {
                    refreshRemoteState();
                    return;
                }
                Q_EMIT informationMessage(message);
            });
    connect(m_remoteReplica.get(),
            &DDMRemoteReplica::loginFailed,
            this,
            &GreeterProxy::onLoginFailed);
    connect(m_remoteReplica.get(),
            &DDMRemoteReplica::userSessionAdded,
            this,
            &GreeterProxy::addUserSession);
    connect(m_remoteReplica.get(),
            &DDMRemoteReplica::userSessionRemoved,
            this,
            &GreeterProxy::removeUserSession);
    connect(m_remoteReplica.get(),
            &QRemoteObjectReplica::stateChanged,
            this,
            &GreeterProxy::remoteStateChanged);
}

void GreeterProxy::onLoginFailed(const QString &user)
{
    qCDebug(treelandGreeter) << "Message received from daemon: LoginFailed" << user;
    Q_EMIT failedAttemptsChanged(++m_failedAttempts);
}

void GreeterProxy::setCanPowerOff(bool canPowerOff)
{
    if (m_canPowerOff == canPowerOff)
        return;
    m_canPowerOff = canPowerOff;
    Q_EMIT canPowerOffChanged(canPowerOff);
}

void GreeterProxy::setCanReboot(bool canReboot)
{
    if (m_canReboot == canReboot)
        return;
    m_canReboot = canReboot;
    Q_EMIT canRebootChanged(canReboot);
}

void GreeterProxy::setCanSuspend(bool canSuspend)
{
    if (m_canSuspend == canSuspend)
        return;
    m_canSuspend = canSuspend;
    Q_EMIT canSuspendChanged(canSuspend);
}

void GreeterProxy::setCanHibernate(bool canHibernate)
{
    if (m_canHibernate == canHibernate)
        return;
    m_canHibernate = canHibernate;
    Q_EMIT canHibernateChanged(canHibernate);
}

void GreeterProxy::setCanHybridSleep(bool canHybridSleep)
{
    if (m_canHybridSleep == canHybridSleep)
        return;
    m_canHybridSleep = canHybridSleep;
    Q_EMIT canHybridSleepChanged(canHybridSleep);
}

void GreeterProxy::remoteStateChanged(QRemoteObjectReplica::State state,
                                      QRemoteObjectReplica::State oldState)
{
    qCInfo(treelandGreeter) << "Greeter remote state changed from" << oldState << "to" << state;
    if (state != QRemoteObjectReplica::Valid)
        return;

    Q_EMIT hostNameChanged(hostName());
    watchRemoteCall("connectGreeter", m_remoteReplica->connectGreeter());
    refreshRemoteState();
}

void GreeterProxy::refreshRemoteState()
{
    if (!m_remoteReplica)
        return;

    updatePowerCapabilities();

    auto *sessionsWatcher = new QRemoteObjectPendingCallWatcher(m_remoteReplica->sessions(), this);
    connect(sessionsWatcher,
            &QRemoteObjectPendingCallWatcher::finished,
            this,
            [](QRemoteObjectPendingCallWatcher *watcher) {
                if (watcher->error() == QRemoteObjectPendingCall::NoError) {
                    const auto sessions = watcher->returnValue().value<QList<SessionEntry>>();
                    qCInfo(treelandGreeter) << "Refreshed sessions from DDM:" << sessions.size();
                    sessionModel()->setSessions(sessions);
                } else {
                    qCWarning(treelandGreeter)
                        << "DDM remote call failed: sessions" << watcher->error();
                }
                watcher->deleteLater();
            });

    auto *lastSessionWatcher =
        new QRemoteObjectPendingCallWatcher(m_remoteReplica->lastSession(), this);
    connect(lastSessionWatcher,
            &QRemoteObjectPendingCallWatcher::finished,
            this,
            [](QRemoteObjectPendingCallWatcher *watcher) {
                if (watcher->error() == QRemoteObjectPendingCall::NoError) {
                    sessionModel()->setLastSession(watcher->returnValue().toString());
                } else {
                    qCWarning(treelandGreeter)
                        << "DDM remote call failed: lastSession" << watcher->error();
                }
                watcher->deleteLater();
            });

    auto *lastUserWatcher = new QRemoteObjectPendingCallWatcher(m_remoteReplica->lastUser(), this);
    connect(lastUserWatcher,
            &QRemoteObjectPendingCallWatcher::finished,
            this,
            [](QRemoteObjectPendingCallWatcher *watcher) {
                if (watcher->error() == QRemoteObjectPendingCall::NoError) {
                    userModel()->setLastUser(watcher->returnValue().toString());
                } else {
                    qCWarning(treelandGreeter)
                        << "DDM remote call failed: lastUser" << watcher->error();
                }
                watcher->deleteLater();
            });

    auto *rememberSessionWatcher =
        new QRemoteObjectPendingCallWatcher(m_remoteReplica->rememberLastSession(), this);
    connect(rememberSessionWatcher,
            &QRemoteObjectPendingCallWatcher::finished,
            this,
            [](QRemoteObjectPendingCallWatcher *watcher) {
                if (watcher->error() == QRemoteObjectPendingCall::NoError) {
                    sessionModel()->setRememberLastSession(watcher->returnValue().toBool());
                } else {
                    qCWarning(treelandGreeter)
                        << "DDM remote call failed: rememberLastSession" << watcher->error();
                }
                watcher->deleteLater();
            });
}

void GreeterProxy::watchPowerCapability(const char *operation,
                                        const QRemoteObjectPendingCall &call,
                                        void (GreeterProxy::*setter)(bool))
{
    auto *watcher = new QRemoteObjectPendingCallWatcher(call, this);
    connect(watcher,
            &QRemoteObjectPendingCallWatcher::finished,
            this,
            [this, operation, setter](QRemoteObjectPendingCallWatcher *watcher) {
                if (watcher->error() == QRemoteObjectPendingCall::NoError) {
                    const QVariant value = watcher->returnValue();
                    if (value.canConvert<bool>()) {
                        (this->*setter)(value.toBool());
                    } else {
                        qCWarning(treelandGreeter)
                            << "DDM remote call returned invalid capability:" << operation << value;
                    }
                } else {
                    qCWarning(treelandGreeter)
                        << "DDM remote call failed:" << operation << watcher->error();
                }
                watcher->deleteLater();
            });
}

void GreeterProxy::watchRemoteCall(const char *operation, const QRemoteObjectPendingCall &call)
{
    auto *watcher = new QRemoteObjectPendingCallWatcher(call, this);
    connect(watcher,
            &QRemoteObjectPendingCallWatcher::finished,
            this,
            [operation](QRemoteObjectPendingCallWatcher *watcher) {
                if (watcher->error() != QRemoteObjectPendingCall::NoError)
                    qCWarning(treelandGreeter)
                        << "DDM remote call failed:" << operation << watcher->error();
                watcher->deleteLater();
            });
}

void GreeterProxy::resetRemote()
{
    m_remoteReplica.reset();
    m_remoteNode.reset();
}
