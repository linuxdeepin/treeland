// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QHash>
#include <QObject>
#include <QString>

#include <DConfig>

class WaylandServer;
class AnimationConfig;
class AppConfig;
class OutputConfig;
class SeatUserDConfig;
class TreelandConfig;
class TreelandUserConfig;

Q_MOC_INCLUDE("treelanduserconfig.hpp")
Q_MOC_INCLUDE("seatuserconfig.hpp")

class DConfigManager : public QObject
{
    Q_OBJECT
public:
    explicit DConfigManager(QObject *parent = nullptr);
    ~DConfigManager() override;

    static DConfigManager *instance();

    bool isInitializeSucceeded() const;
    bool isInitializeFailed() const;

    TreelandConfig *globalConfig() const;
    AnimationConfig *animationConfig(const QString &userName);
    AnimationConfig *initialAnimationConfig() const;
    TreelandUserConfig *userConfig(const QString &userName);
    SeatUserDConfig *userSeatConfig(const QString &userName, const QString &seatName);
    OutputConfig *outputConfig(const QString &outputName);
    AppConfig *appConfig(const QString &appId);

    // Creates and caches the initial user's configs. Returns true only when both
    // configs have already finished successfully; callers should listen for
    // their initialization signals when this returns false.
    bool initializeUserConfigs(const QString &userName);
    TreelandUserConfig *initialUserConfig() const;

    // Observes active-user changes; the desktop remains responsible for applying
    // the ready configuration to input, wallpaper, cursor and workspace services.
    void initializeActiveUserConfig(WaylandServer *server);
    void stopActiveUserConfig();

Q_SIGNALS:
    void configReady(const QString &username, TreelandUserConfig *config, SeatUserDConfig *seatConfig);
    void initializeSucceed();
    void initializeFailed();

private:
    void updateCurrentUser();
    void onPendingUserConfigInitialized();
    void onGlobalConfigInitializeSucceed(DTK_CORE_NAMESPACE::DConfig *config);
    void onGlobalConfigInitializeFailed();

    static DConfigManager *s_instance;

    TreelandConfig *m_globalConfig = nullptr;
    QHash<QString, AnimationConfig *> m_animationConfigs;
    AnimationConfig *m_initialAnimationConfig = nullptr;
    QHash<QString, TreelandUserConfig *> m_userConfigs;
    QHash<QString, SeatUserDConfig *> m_seatUserConfigs;
    QHash<QString, OutputConfig *> m_outputConfigs;
    QHash<QString, AppConfig *> m_appConfigs;
    TreelandUserConfig *m_initialUserConfig = nullptr;

    WaylandServer *m_server = nullptr;
    QMetaObject::Connection m_currentUserChangedConnection;
    TreelandUserConfig *m_pendingUserConfig = nullptr;
    SeatUserDConfig *m_pendingSeatConfig = nullptr;
    QString m_pendingUserName;
};
