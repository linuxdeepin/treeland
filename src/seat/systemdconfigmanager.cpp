// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "systemdconfigmanager.h"

#include "common/treelandlogging.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"

#include <QEventLoop>
#include <QTimer>

namespace {
// The generated config classes renamed isInitializeSucceed() to
// isInitializeSucceeded() once the dconfig file minor version became > 0, so
// guard the call site the same way the rest of the codebase does.
bool globalConfigReady(TreelandConfig *config)
{
#if TREELANDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    return config->isInitializeSucceeded();
#else
    return config->isInitializeSucceed();
#endif
}

bool userConfigReady(TreelandUserConfig *config)
{
#if TREELANDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    return config->isInitializeSucceeded();
#else
    return config->isInitializeSucceed();
#endif
}
} // namespace

SystemDConfigManager::SystemDConfigManager(QObject *parent)
    : QObject(parent)
{
    m_globalConfig.reset(TreelandConfig::create("org.deepin.dde.treeland", QString()));
    // Stand-in config used before a real session path is known; the real path
    // is applied in resetUserConfig() and only that one is awaited.
    m_userConfig.reset(TreelandUserConfig::createByName("org.deepin.dde.treeland.user",
                                                        "org.deepin.dde.treeland",
                                                        "/dde"));
    // The blocking wait lives in initialize() so Helper can store the pointer
    // first; see initialize() and the class doc on isInitializeSucceeded().
}

SystemDConfigManager::~SystemDConfigManager() = default;

void SystemDConfigManager::initialize()
{
    m_globalConfigInitialized = waitForGlobalConfigInitialized();
    checkInitialized();
}

bool SystemDConfigManager::isInitializeSucceeded() const
{
    return m_globalConfigInitialized && m_userConfigInitialized;
}

TreelandConfig *SystemDConfigManager::globalConfig() const
{
    return m_globalConfig.get();
}

TreelandUserConfig *SystemDConfigManager::userConfig() const
{
    return m_userConfig.get();
}

void SystemDConfigManager::resetUserConfig(const QString &userName)
{
    m_userConfig.reset(TreelandUserConfig::createByName("org.deepin.dde.treeland.user",
                                                        "org.deepin.dde.treeland",
                                                        "/" + userName));
    m_userConfigInitialized = false;

    // The "dde" path is only a placeholder for the greeter, so do not block on
    // it; only a real session config is awaited.
    if (userName != "dde") {
        m_userConfigInitialized = waitForUserConfigInitialized();
    }

    checkInitialized();
}

bool SystemDConfigManager::waitForGlobalConfigInitialized()
{
    // Already loaded successfully or already reported failure: either way the
    // wait is done, and only success counts as initialized.
    if (globalConfigReady(m_globalConfig.get())) {
        return true;
    }
    if (m_globalConfig->isInitializeFailed()) {
        return false;
    }

    bool success = false;
    QEventLoop loop;
    connect(m_globalConfig.get(), &TreelandConfig::configInitializeSucceed, &loop, [&success, &loop] {
        success = true;
        loop.quit();
    });
    connect(m_globalConfig.get(), &TreelandConfig::configInitializeFailed, &loop, [&success, &loop] {
        success = false;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, [&success, &loop] {
        qCWarning(lcTlConfig) << "Global dconfig initialization timed out after 5s, using defaults";
        success = false;
        loop.quit();
    });
    loop.exec();
    return success;
}

bool SystemDConfigManager::waitForUserConfigInitialized()
{
    // Mirror the global path: succeed, fail, or timeout all terminate the
    // wait instead of forcing a full 5s timeout on failure.
    if (userConfigReady(m_userConfig.get())) {
        return true;
    }
    if (m_userConfig->isInitializeFailed()) {
        return false;
    }

    bool success = false;
    QEventLoop loop;
    connect(m_userConfig.get(), &TreelandUserConfig::configInitializeSucceed, &loop, [&success, &loop] {
        success = true;
        loop.quit();
    });
    connect(m_userConfig.get(), &TreelandUserConfig::configInitializeFailed, &loop, [&success, &loop] {
        success = false;
        loop.quit();
    });
    QTimer::singleShot(5000, &loop, [&success, &loop] {
        qCWarning(lcTlConfig) << "User dconfig initialization timed out after 5s, using defaults";
        success = false;
        loop.quit();
    });
    loop.exec();
    return success;
}

void SystemDConfigManager::checkInitialized()
{
    if (m_globalConfigInitialized && m_userConfigInitialized && !m_initializeSucceedEmitted) {
        m_initializeSucceedEmitted = true;
        Q_EMIT initializeSucceed();
    }
}
