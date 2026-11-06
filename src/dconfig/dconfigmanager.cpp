// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "dconfigmanager.h"
#include "core/waylandserver.h"
#include "greeter/usermodel.h"

#include "animationconfig.hpp"
#include "appconfig.hpp"
#include "outputconfig.hpp"
#include "seatuserconfig.hpp"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"

#include "common/treelandlogging.h"
#include "seat/seatmanager.h"

#include <wseat.h>

#include <DConfig>

namespace {

static bool userConfigInitializationFinished(TreelandUserConfig *config)
{
    return config && (config->isInitializeSucceeded() || config->isInitializeFailed());
}

static bool seatConfigInitializationFinished(SeatUserDConfig *config)
{
    return config && (config->isInitializeSucceeded() || config->isInitializeFailed());
}


QString configSubpath(const QString &name)
{
    return QStringLiteral("/") + name;
}

QString seatConfigSubpath(const QString &userName, const QString &seatName)
{
    return configSubpath(userName) + QStringLiteral("/") + seatName;
}

}

DConfigManager *DConfigManager::s_instance = nullptr;

DConfigManager::DConfigManager(QObject *parent)
    : QObject(parent)
{
    Q_ASSERT(!s_instance);
    s_instance = this;

    m_globalConfig = TreelandConfig::create(QStringLiteral("org.deepin.dde.treeland"),
                                             QString(),
                                             this);
    Q_ASSERT(m_globalConfig);

    connect(m_globalConfig,
            &TreelandConfig::configInitializeSucceed,
            this,
            &DConfigManager::onGlobalConfigInitializeSucceed,
            Qt::SingleShotConnection);
    connect(m_globalConfig,
            &TreelandConfig::configInitializeFailed,
            this,
            &DConfigManager::onGlobalConfigInitializeFailed,
            Qt::SingleShotConnection);

    qCInfo(lcTlConfig) << "DConfig manager created; waiting for global configuration initialization";
}

DConfigManager::~DConfigManager()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

DConfigManager *DConfigManager::instance()
{
    return s_instance;
}

void DConfigManager::onGlobalConfigInitializeSucceed(DTK_CORE_NAMESPACE::DConfig *)
{
    qCInfo(lcTlConfig) << "Global DConfig initialization succeeded";
    Q_EMIT initializeSucceed();
}

void DConfigManager::onGlobalConfigInitializeFailed()
{
    qCCritical(lcTlConfig) << "Global DConfig initialization failed";
    Q_EMIT initializeFailed();
}

bool DConfigManager::isInitializeSucceeded() const
{
    return m_globalConfig && m_globalConfig->isInitializeSucceeded();
}

bool DConfigManager::isInitializeFailed() const
{
    return !m_globalConfig || m_globalConfig->isInitializeFailed();
}

TreelandConfig *DConfigManager::globalConfig() const
{
    return m_globalConfig;
}

AnimationConfig *DConfigManager::animationConfig(const QString &userName)
{
    if (userName.isEmpty())
        return nullptr;

    if (auto *config = m_animationConfigs.value(userName))
        return config;

    auto *config = AnimationConfig::createByName(QStringLiteral("org.deepin.dde.treeland.animation"),
                                                QStringLiteral("org.deepin.dde.treeland"),
                                                configSubpath(userName),
                                                this);
    m_animationConfigs.insert(userName, config);
    return config;
}

AnimationConfig *DConfigManager::initialAnimationConfig() const
{
    return m_initialAnimationConfig;
}

TreelandUserConfig *DConfigManager::userConfig(const QString &userName)
{
    if (userName.isEmpty()) {
        return nullptr;
    }

    if (auto *config = m_userConfigs.value(userName)) {
        return config;
    }

    auto *config = TreelandUserConfig::createByName(QStringLiteral("org.deepin.dde.treeland.user"),
                                                    QStringLiteral("org.deepin.dde.treeland"),
                                                    configSubpath(userName),
                                                    this);
    m_userConfigs.insert(userName, config);
    return config;
}

SeatUserDConfig *DConfigManager::userSeatConfig(const QString &userName, const QString &seatName)
{
    if (userName.isEmpty() || seatName.isEmpty()) {
        return nullptr;
    }

    const auto key = seatConfigSubpath(userName, seatName);
    if (auto *config = m_seatUserConfigs.value(key)) {
        return config;
    }

    auto *config = SeatUserDConfig::createByName(
        QStringLiteral("org.deepin.dde.treeland.user.seat"),
        QStringLiteral("org.deepin.dde.treeland"),
        seatConfigSubpath(userName, seatName),
        this);
    m_seatUserConfigs.insert(key, config);
    return config;
}

OutputConfig *DConfigManager::outputConfig(const QString &outputName)
{
    if (outputName.isEmpty()) {
        return nullptr;
    }

    if (auto *config = m_outputConfigs.value(outputName)) {
        return config;
    }

    auto *config = OutputConfig::createByName(QStringLiteral("org.deepin.dde.treeland.output"),
                                              QStringLiteral("org.deepin.dde.treeland"),
                                              configSubpath(outputName),
                                              this);
    m_outputConfigs.insert(outputName, config);
    return config;
}

AppConfig *DConfigManager::appConfig(const QString &appId)
{
    if (appId.isEmpty()) {
        return nullptr;
    }

    if (auto *config = m_appConfigs.value(appId)) {
        return config;
    }

    auto *config = AppConfig::create(QStringLiteral("org.deepin.dde.treeland"),
                                     configSubpath(appId),
                                     this);
    m_appConfigs.insert(appId, config);
    return config;
}

bool DConfigManager::initializeUserConfigs(const QString &userName)
{
    m_initialAnimationConfig = animationConfig(userName);
    m_initialUserConfig = userConfig(userName);
    if (!m_initialUserConfig || !m_initialUserConfig->isInitializeSucceeded())
        return false;

    const auto seats = WaylandServer::instance()->seatManager()->seats();
    for (WSeat *seat : seats) {
        const auto *seatConfig = userSeatConfig(userName, seat->name());
        if (!seatConfig || !seatConfig->isInitializeSucceeded())
            return false;
    }

    return true;
}

TreelandUserConfig *DConfigManager::initialUserConfig() const
{
    return m_initialUserConfig;
}

void DConfigManager::initializeActiveUserConfig(WaylandServer *server)
{
    m_server = server;
    m_currentUserChangedConnection = connect(m_server->userModel(), &UserModel::currentUserNameChanged,
                                            this, &DConfigManager::updateCurrentUser);
    updateCurrentUser();
}

void DConfigManager::stopActiveUserConfig()
{
    QObject::disconnect(m_currentUserChangedConnection);
    if (m_pendingUserConfig)
        QObject::disconnect(static_cast<const QObject *>(m_pendingUserConfig), nullptr, this, nullptr);
    if (m_pendingSeatConfig)
        QObject::disconnect(static_cast<const QObject *>(m_pendingSeatConfig), nullptr, this, nullptr);
    m_pendingUserName.clear();
    m_pendingUserConfig = nullptr;
    m_pendingSeatConfig = nullptr;
    m_server = nullptr;
}

void DConfigManager::updateCurrentUser()
{
    if (!m_server)
        return;

    const QString userName = m_server->userModel()->currentUserName();
    auto *configManager = DConfigManager::instance();
    auto *userConfig = configManager ? configManager->userConfig(userName) : m_server->config();
    WSeat *defaultSeat = m_server->seatManager()->getSeat(DEFAULT_SEAT_NAME);
    const QString seatName = defaultSeat ? defaultSeat->name() : QStringLiteral(DEFAULT_SEAT_NAME);
    auto *seatConfig = configManager ? configManager->userSeatConfig(userName, seatName) : nullptr;
    if (!userConfig) {
        qCWarning(lcTlConfig) << "Cannot switch to user" << userName
                              << "because its DConfig object is unavailable";
        return;
    }

    if (userConfig == m_server->config() && (!seatConfig || seatConfig == m_pendingSeatConfig)) {
        Q_EMIT configReady(userName, userConfig, seatConfig);
        return;
    }

    if (m_pendingUserConfig) {
        QObject::disconnect(static_cast<const QObject *>(m_pendingUserConfig),
                            nullptr,
                            this,
                            nullptr);
    }
    if (m_pendingSeatConfig) {
        QObject::disconnect(static_cast<const QObject *>(m_pendingSeatConfig),
                            nullptr,
                            this,
                            nullptr);
    }

    m_pendingUserName = userName;
    m_pendingUserConfig = userConfig;
    m_pendingSeatConfig = seatConfig;

    if (!userConfigInitializationFinished(userConfig)) {
        connect(userConfig,
                &TreelandUserConfig::configInitializeSucceed,
                this,
                &DConfigManager::onPendingUserConfigInitialized,
                Qt::SingleShotConnection);
        connect(userConfig,
                &TreelandUserConfig::configInitializeFailed,
                this,
                &DConfigManager::onPendingUserConfigInitialized,
                Qt::SingleShotConnection);
    }
    if (seatConfig && !seatConfigInitializationFinished(seatConfig)) {
        connect(seatConfig,
                &SeatUserDConfig::configInitializeSucceed,
                this,
                &DConfigManager::onPendingUserConfigInitialized,
                Qt::SingleShotConnection);
        connect(seatConfig,
                &SeatUserDConfig::configInitializeFailed,
                this,
                &DConfigManager::onPendingUserConfigInitialized,
                Qt::SingleShotConnection);
    }

    qCInfo(lcTlConfig) << "Waiting for user DConfig initialization before switching to"
                       << userName;
    onPendingUserConfigInitialized();
}

void DConfigManager::onPendingUserConfigInitialized()
{
    if (!m_server)
        return;

    if (sender() && sender() != m_pendingUserConfig && sender() != m_pendingSeatConfig) {
        return;
    }

    if (!userConfigInitializationFinished(m_pendingUserConfig)
        || (m_pendingSeatConfig && !seatConfigInitializationFinished(m_pendingSeatConfig))) {
        return;
    }

    const QString userName = m_pendingUserName;
    auto *userConfig = m_pendingUserConfig;
    auto *seatConfig = m_pendingSeatConfig;
    m_pendingUserName.clear();
    m_pendingUserConfig = nullptr;
    m_pendingSeatConfig = nullptr;

    if (m_server->userModel()->currentUserName() != userName) {
        qCInfo(lcTlConfig) << "Discarding stale user DConfig initialization for" << userName;
        return;
    }

    qCInfo(lcTlConfig) << "User DConfig initialization finished for" << userName
                       << "; applying user configuration";
    if (userConfig->isInitializeFailed()
        || (seatConfig && seatConfig->isInitializeFailed())) {
        qCWarning(lcTlConfig) << "Using generated defaults for user DConfig" << userName;
    }
    Q_EMIT configReady(userName, userConfig, seatConfig);
}
