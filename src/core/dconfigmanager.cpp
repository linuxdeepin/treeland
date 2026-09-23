// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "dconfigmanager.h"

#include "appconfig.hpp"
#include "outputconfig.hpp"
#include "seatuserconfig.hpp"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"

#include "common/treelandlogging.h"
#include "seat/helper.h"
#include "seat/seatmanager.h"

#include <wseat.h>

#include <DConfig>

namespace {

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
    m_initialUserConfig = userConfig(userName);
    if (!m_initialUserConfig || !m_initialUserConfig->isInitializeSucceeded())
        return false;

    const auto seats = Helper::instance()->seatManager()->seats();
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
