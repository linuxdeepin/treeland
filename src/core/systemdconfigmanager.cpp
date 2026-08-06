// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "systemdconfigmanager.h"

#include "appconfig.hpp"
#include "outputconfig.hpp"
#include "seatuserconfig.hpp"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"

#include <DConfig>

namespace {

QString configSubpath(const QString &name)
{
    return QStringLiteral("/") + name;
}

}

SystemDConfigManager *SystemDConfigManager::s_instance = nullptr;

SystemDConfigManager::SystemDConfigManager(QObject *parent)
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
            [this](DTK_CORE_NAMESPACE::DConfig *) { Q_EMIT InitializeSucceed(); },
            Qt::SingleShotConnection);
    connect(m_globalConfig,
            &TreelandConfig::configInitializeFailed,
            this,
            [this] { Q_EMIT InitializeFailed(); },
            Qt::SingleShotConnection);
}

SystemDConfigManager::~SystemDConfigManager()
{
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

SystemDConfigManager *SystemDConfigManager::instance()
{
    return s_instance;
}

bool SystemDConfigManager::isInitializeSucceeded() const
{
    return m_globalConfig && m_globalConfig->isInitializeSucceeded();
}

bool SystemDConfigManager::isInitializeFailed() const
{
    return !m_globalConfig || m_globalConfig->isInitializeFailed();
}

TreelandConfig *SystemDConfigManager::globalConfig() const
{
    return m_globalConfig;
}

TreelandUserConfig *SystemDConfigManager::userConfig(const QString &userName)
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

SeatUserDConfig *SystemDConfigManager::seatUserConfig(const QString &userName)
{
    if (userName.isEmpty()) {
        return nullptr;
    }

    if (auto *config = m_seatUserConfigs.value(userName)) {
        return config;
    }

    auto *config = SeatUserDConfig::createByName(
        QStringLiteral("org.deepin.dde.treeland.user.seat"),
        QStringLiteral("org.deepin.dde.treeland"),
        configSubpath(userName),
        this);
    m_seatUserConfigs.insert(userName, config);
    return config;
}

OutputConfig *SystemDConfigManager::outputConfig(const QString &outputName)
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

AppConfig *SystemDConfigManager::appConfig(const QString &appId)
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

bool SystemDConfigManager::initializeUserConfigs(const QString &userName)
{
    m_initialUserConfig = userConfig(userName);
    const auto *seatConfig = seatUserConfig(userName);
    return m_initialUserConfig && seatConfig
        && m_initialUserConfig->isInitializeSucceeded()
        && seatConfig->isInitializeSucceeded();
}

TreelandUserConfig *SystemDConfigManager::initialUserConfig() const
{
    return m_initialUserConfig;
}
