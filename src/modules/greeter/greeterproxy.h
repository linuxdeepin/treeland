// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QRemoteObjectPendingCall>
#include <QRemoteObjectReplica>

#include <memory>

class QRemoteObjectNode;
class QUrl;
class DDMRemoteReplica;
class LockScreen;

class GreeterProxy : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(GreeterProxy)

    QML_NAMED_ELEMENT(GreeterProxy)
    QML_SINGLETON

    ////////////////
    // Properties //
    ////////////////

    Q_PROPERTY(QString hostName         READ hostName         NOTIFY hostNameChanged)
    Q_PROPERTY(bool    canPowerOff      READ canPowerOff      NOTIFY canPowerOffChanged)
    Q_PROPERTY(bool    canReboot        READ canReboot        NOTIFY canRebootChanged)
    Q_PROPERTY(bool    canSuspend       READ canSuspend       NOTIFY canSuspendChanged)
    Q_PROPERTY(bool    canHibernate     READ canHibernate     NOTIFY canHibernateChanged)
    Q_PROPERTY(bool    canHybridSleep   READ canHybridSleep   NOTIFY canHybridSleepChanged)

    Q_PROPERTY(bool    isLocked         READ isLocked         NOTIFY lockChanged)
    Q_PROPERTY(int     failedAttempts   READ failedAttempts   NOTIFY failedAttemptsChanged)
    Q_PROPERTY(bool    showShutdownView READ showShutdownView WRITE  setShowShutdownView NOTIFY showShutdownViewChanged)
    Q_PROPERTY(bool    showAnimation    READ showAnimation    NOTIFY showAnimationChanged)
    Q_PROPERTY(bool    hasActiveSession READ hasActiveSession NOTIFY hasActiveSessionChanged)

public:
    explicit GreeterProxy(QObject *parent = nullptr);
    ~GreeterProxy();

    //////////////////////
    // Property getters //
    //////////////////////

    /**
     * @brief Get the host name
     * @return Host name
     */
    QString hostName() const;

    /**
     * @brief Get the power off capability
     * @return true if can power off
     */
    bool canPowerOff() const;

    /**
     * @brief Get the reboot capability
     * @return true if can reboot
     */
    bool canReboot() const;

    /**
     * @brief Get the suspend capability
     * @return true if can suspend
     */
    bool canSuspend() const;

    /**
     * @brief Get the hibernate capability
     * @return true if can hibernate
     */
    bool canHibernate() const;

    /**
     * @brief Get the hybrid sleep capability
     * @return true if can hybrid sleep
     */
    bool canHybridSleep() const;

    /**
     * @brief Get the greeter's lock state
     * QML elements should listen to this property to show/hide the lock screen
     *
     * @return true if is locked
     */
    bool isLocked() const;

    /**
     * @brief Get the number of failed login attempts (password incorrect)
     * The value is reset to 0 when unlocked successfully
     * QML elements should listen to this property to detect failed login/unlock attempts
     *
     * @return Number of failed attempts
     */
    inline int failedAttempts() const
    {
        return m_failedAttempts;
    };

    /**
     * @brief Get whether the shutdown view is shown
     * QML elements should listen to this property to show/hide the shutdown view
     *
     * @return true if shutdown view is shown
     */
    inline bool showShutdownView() const
    {
        return m_showShutdownView;
    };

    /**
     * @brief Get whether to show animation on lock/unlock
     * QML elements should listen to this property to enable/disable animation
     *
     * @return true if show animation
     */
    inline bool showAnimation() const
    {
        return m_showAnimation;
    };

    /**
     * @brief Get whether there is an active user session
     * QML elements should listen to this property to show/hide session related UI
     *
     * @return true if has active session
     */
    inline bool hasActiveSession() const
    {
        return m_hasActiveSession;
    };

    /////////////////////////////
    // Public property setters //
    /////////////////////////////

    /**
     * @brief Set whether to show the shutdown view
     * QML elements should set this property to show/hide the shutdown view
     *
     * @param show true to show shutdown view, false to hide
     */
    void setShowShutdownView(bool show);

    /**
     * @brief Set the local lock state without sending a request to DDM.
     * @param isLocked true to show lockscreen, false to hide it
     */
    void setLock(bool isLocked);

    ////////////////////
    // Public methods //
    ////////////////////

    /**
     * @brief Check if the DDM socket is connected
     * @return true if connected
     */
    bool isConnected() const;

    /**
     * @brief Set the LockScreen instance
     * This is necessary for the GreeterProxy to control the lock screen visibility
     *
     * @param lockScreen LockScreen instance
     */
    void setLockScreen(LockScreen *lockScreen);

public Q_SLOTS:

    /////////////
    // Actions //
    /////////////

    /** @brief Power off the system. Need to be connected with DDM. */
    void powerOff();

    /** @brief Reboot the system. Need to be connected with DDM. */
    void reboot();

    /** @brief Suspend the system. Need to be connected with DDM. */
    void suspend();

    /** @brief Hibernate the system. Need to be connected with DDM. */
    void hibernate();

    /** @brief Hybrid sleep the system. Need to be connected with DDM. */
    void hybridSleep();

    /** @brief Login given user with given password for given desktop session.
     * This function will call DDM to perform the login.
     *
     * Listen to user session signals from DDM to detect session changes,
     * and listen to failedAttempts property to detect failed login attempts.
     *
     * @param user Username
     * @param password Password
     * @param sessionIndex Index of the desktop session in the session model
     */
    void login(const QString &user, const QString &password, int sessionIndex);

    /** @brief Lock the current active session.
     * This function will call DDM to perform the lock.
     *
     * Show the lockscreen locally (Treeland self-manages lock state).
     */
    void lock();

    /** @brief Logout the current active session.
     * This function will call DDM to perform the logout.
     *
     * Listen to userSessionRemoved from DDM to detect if the session is
     * successfully logged out.
     */
    void logout();

    /** @brief Refresh power capability cache from DDM asynchronously. */
    void updatePowerCapabilities();
    void setDDMConnectionEnabled(bool enabled);
    void connectToDDM();

private Q_SLOTS:

    ///////////////////////

    // DDM Communication //
    ///////////////////////

Q_SIGNALS:
    void informationMessage(const QString &message);

    void switchUser();

    void socketConnected();
    void socketDisconnected();

    /////////////////////////////
    // Property change signals //
    /////////////////////////////

    /** @brief Emitted when host name changes. See hostName() */
    void hostNameChanged(const QString &hostName);

    /** @brief Emitted when power off capability changes. See canPowerOff() */
    void canPowerOffChanged(bool canPowerOff);

    /** @brief Emitted when reboot capability changes. See canReboot() */
    void canRebootChanged(bool canReboot);

    /** @brief Emitted when suspend capability changes. See canSuspend() */
    void canSuspendChanged(bool canSuspend);

    /** @brief Emitted when hibernate capability changes. See canHibernate() */
    void canHibernateChanged(bool canHibernate);

    /** @brief Emitted when hybrid sleep capability changes. See canHybridSleep() */
    void canHybridSleepChanged(bool canHybridSleep);

    /** @brief Emitted when lock state changes. See isLocked() */
    void lockChanged(bool isLocked);

    /** @brief Emitted when failed attempts changes. See failedAttempts() */
    void failedAttemptsChanged(int failedAttempts);

    /** @brief Emitted when showShutdownView changes. See showShutdownView() */
    void showShutdownViewChanged(bool showShutdownView);

    /** @brief Emitted when showAnimation changes. See showAnimation() */
    void showAnimationChanged(bool showAnimation);

    /** @brief Emitted when hasActiveSession changes. See hasActiveSession() */
    void hasActiveSessionChanged(bool hasActiveSession);

private:
    /////////////////////
    // Private methods //
    /////////////////////

    static QUrl configuredDdmRemoteUrl();
    void resetRemote();
    void onLoginFailed(const QString &user);
    void addUserSession(const QString &user, int sessionId);
    void removeUserSession(const QString &user, int sessionId);
    void remoteStateChanged(QRemoteObjectReplica::State state,
                            QRemoteObjectReplica::State oldState);
    void refreshRemoteState();
    void setCanPowerOff(bool canPowerOff);
    void setCanReboot(bool canReboot);
    void setCanSuspend(bool canSuspend);
    void setCanHibernate(bool canHibernate);
    void setCanHybridSleep(bool canHybridSleep);
    void watchRemoteCall(const char *operation, const QRemoteObjectPendingCall &call);
    void watchPowerCapability(const char *operation,
                              const QRemoteObjectPendingCall &call,
                              void (GreeterProxy::*setter)(bool));

    /////////////////////
    // Property values //
    /////////////////////

    std::unique_ptr<QRemoteObjectNode> m_remoteNode;
    std::unique_ptr<DDMRemoteReplica> m_remoteReplica;
    LockScreen *m_lockScreen{ nullptr };
    QMetaObject::Connection m_lockScreenVisibleConnection;

    int m_failedAttempts{ 0 };
    bool m_showShutdownView{ false };
    bool m_showAnimation{ true };
    bool m_hasActiveSession{ false };
    bool m_canPowerOff{ false };
    bool m_canReboot{ false };
    bool m_canSuspend{ false };
    bool m_canHibernate{ false };
    bool m_canHybridSleep{ false };
    bool m_ddmConnectionEnabled{ false };
};
