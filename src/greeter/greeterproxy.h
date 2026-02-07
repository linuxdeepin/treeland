// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "QDBusContext"
#include <QDBusObjectPath>
#include <QObject>
#include <QQmlEngine>

class QLocalSocket;

class LockScreen;

class GreeterProxy
    : public QObject
    , protected QDBusContext
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
    inline const QString &hostName() const { return m_hostName; };

    /**
     * @brief Get the power off capability
     * @return true if can power off
     */
    inline bool canPowerOff()      const { return m_canPowerOff;      }; 

    /**
     * @brief Get the reboot capability
     * @return true if can reboot
     */
    inline bool canReboot()        const { return m_canReboot;        }; 

    /**
     * @brief Get the suspend capability
     * @return true if can suspend
     */
    inline bool canSuspend()       const { return m_canSuspend;       }; 

    /**
     * @brief Get the hibernate capability
     * @return true if can hibernate
     */
    inline bool canHibernate()     const { return m_canHibernate;     }; 

    /**
     * @brief Get the hybrid sleep capability
     * @return true if can hybrid sleep
     */
    inline bool canHybridSleep()   const { return m_canHybridSleep;   };

    /**
     * @brief Get the greeter's lock state
     * QML elements should listen to this property to show/hide the lock screen
     *
     * @return true if is locked
     */
    inline bool isLocked()         const { return m_isLocked;         };

    /**
     * @brief Get the number of failed login attempts (password incorrect)
     * The value is reset to 0 when unlocked successfully
     * QML elements should listen to this property to detect failed login/unlock attempts
     *
     * @return Number of failed attempts
     */
    inline int  failedAttempts()   const { return m_failedAttempts;   };

    /**
     * @brief Get whether the shutdown view is shown
     * QML elements should listen to this property to show/hide the shutdown view
     *
     * @return true if shutdown view is shown
     */
    inline bool showShutdownView() const { return m_showShutdownView; };

    /**
     * @brief Get whether to show animation on lock/unlock
     * QML elements should listen to this property to enable/disable animation
     *
     * @return true if show animation
     */
    inline bool showAnimation()    const { return m_showAnimation;    };

    /**
     * @brief Get whether there is an active user session
     * QML elements should listen to this property to show/hide session related UI
     *
     * @return true if has active session
     */
    inline bool hasActiveSession() const { return m_hasActiveSession; };

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
    inline void setLockScreen(LockScreen *lockScreen) { m_lockScreen = lockScreen; };

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
     * Listen to org.freedesktop.login1.Manager.SessionNew signal to
     * detect if new sessions successfully logged in, and listen to
     * failedAttempts property to detect failed login attempts.
     *
     * @param user Username
     * @param password Password
     * @param sessionIndex Index of the desktop session in the session model
     */
    void login(const QString &user, const QString &password, int sessionIndex);

    /** @brief Lock the current active session.
     * This function will call DDM to perform the lock.
     *
     * Listen to org.freedesktop.login1.Session.Lock signal to detect
     * if the session is successfully locked.
     */
    void lock();

    /** @brief Unlock given user with given password.
     * This function will call DDM to perform the unlock.
     *
     * Listen to org.freedesktop.login1.Session.Unlock signal to
     * detect if the session is successfully unlocked, and listen to
     * failedAttempts property to detect failed unlock attempts.
     *
     * @param user Username
     * @param password Password
     */
    void unlock(const QString &user, const QString &password);

    /** @brief Logout the current active session.
     * This function will call DDM to perform the logout.
     *
     * Listen to org.freedesktop.login1.Manager.SessionRemoved signal to
     * detect if the session is successfully logged out.
     */
    void logout();

private Q_SLOTS:

    ///////////////////////
    // DDM Communication //
    ///////////////////////

    void connected();
    void disconnected();
    void readyRead();
    void error();

    //////////////////////////////
    // Logind session listeners //
    //////////////////////////////

    /** @brief Listener for org.freederktop.login1.Manager.SessionNew */
    void onSessionNew(const QString &id, const QDBusObjectPath &session);

    /** @brief Listener for org.freederktop.login1.Manager.SessionRemoved */
    void onSessionRemoved(const QString &id, const QDBusObjectPath &session);

    /** @brief Listener for org.freederktop.login1.Session.Lock */
    void onSessionLock();

    /** @brief Listener for org.freederktop.login1.Session.Unlock */
    void onSessionUnlock();

Q_SIGNALS:
    void informationMessage(const QString &message);

    void switchUser();

    void socketDisconnected();

    /////////////////////////////
    // Property change signals //
    /////////////////////////////

    /** @brief Emitted when host name changes. See hostName() */
    void hostNameChanged(const QString &hostName);

    /** @brief Emitted when power off capability changes. See canPowerOff() */
    void canPowerOffChanged      (bool canPowerOff);

    /** @brief Emitted when reboot capability changes. See canReboot() */
    void canRebootChanged        (bool canReboot);

    /** @brief Emitted when suspend capability changes. See canSuspend() */
    void canSuspendChanged       (bool canSuspend);

    /** @brief Emitted when hibernate capability changes. See canHibernate() */
    void canHibernateChanged     (bool canHibernate);

    /** @brief Emitted when hybrid sleep capability changes. See canHybridSleep() */
    void canHybridSleepChanged   (bool canHybridSleep);

    /** @brief Emitted when lock state changes. See isLocked() */
    void lockChanged             (bool isLocked);

    /** @brief Emitted when failed attempts changes. See failedAttempts() */
    void failedAttemptsChanged   (int failedAttempts);

    /** @brief Emitted when showShutdownView changes. See showShutdownView() */
    void showShutdownViewChanged (bool showShutdownView);

    /** @brief Emitted when showAnimation changes. See showAnimation() */
    void showAnimationChanged    (bool showAnimation);

    /** @brief Emitted when hasActiveSession changes. See hasActiveSession() */
    void hasActiveSessionChanged (bool hasActiveSession);

private:

    /////////////////////
    // Private methods //
    /////////////////////

    /**
     * @brief Set the lock state
     * This is the internal method to set the lock state (lockscreen
     * visibility, etc.) directly without calling DDM and
     * communicating with systemd-logind.
     *
     * @param isLocked true to set locked, false to set unlocked
     */
    void setLock(bool isLocked);

    /**
     * @brief Update the DDM communication socket
     */
    void updateAuthSocket();

    /////////////////////
    // Property values //
    /////////////////////

    QLocalSocket *m_socket{ nullptr };
    LockScreen *m_lockScreen{ nullptr };

    QString m_hostName{};

    bool m_canPowerOff      { false };
    bool m_canReboot        { false };
    bool m_canSuspend       { false };
    bool m_canHibernate     { false };
    bool m_canHybridSleep   { false };

    bool m_isLocked         { false };
    int  m_failedAttempts   { 0     };
    bool m_showShutdownView { false };
    bool m_showAnimation    { true  };
    bool m_hasActiveSession { false };
};
