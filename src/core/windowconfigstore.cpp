// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/windowconfigstore.h"

#include "appconfig.hpp"
#include "common/treelandlogging.h"

#include <QPointer>

WindowConfigStore::WindowConfigStore(QObject *parent)
    : QObject(parent)
{
}

AppConfig *WindowConfigStore::configForApp(const QString &appId) const
{
    if (appId.isEmpty()) {
        return nullptr;
    }

    if (auto *config = m_appConfigs.value(appId)) {
        return config;
    }

    auto *config = AppConfig::create(QStringLiteral("org.deepin.dde.treeland"),
                                     "/" + appId,
                                     const_cast<WindowConfigStore *>(this));
    m_appConfigs.insert(appId, config);
    return config;
}

void WindowConfigStore::saveLastSize(const QString &appId, const QSize &size)
{
    if (appId.isEmpty() || !size.isValid()) {
        qCWarning(treelandCore) << "WindowConfigStore: saveLastSize invalid parameters for" << appId
                                << size;
        return;
    }

    auto *config = configForApp(appId);
    if (!config) {
        qCWarning(treelandCore) << "WindowConfigStore: saveLastSize no config for" << appId;
        return;
    }

    qCDebug(treelandCore) << "WindowConfigStore: save size for" << appId << "as" << size;
    config->setLastWindowWidth(size.width());
    config->setLastWindowHeight(size.height());
}

void WindowConfigStore::withSplashConfigFor(
    const QString &appId,
    QObject *context,
    std::function<void(const QSize &size,
                       const QString &darkPalette,
                       const QString &lightPalette,
                       qlonglong splashThemeType)> callback,
    std::function<void()> skipCallback,
    std::function<void()> waitCallback) const
{
    Q_ASSERT_X(callback, Q_FUNC_INFO, "callback must be provided");
    Q_ASSERT_X(skipCallback, Q_FUNC_INFO, "skipCallback must be provided");
    Q_ASSERT_X(waitCallback, Q_FUNC_INFO, "waitCallback must be provided");
    qCDebug(treelandCore) << "WindowConfigStore: withSplashConfigFor requested for" << appId;
    auto *config = configForApp(appId);
    if (!config) {
        skipCallback();
        return;
    }
#if APPCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    if (config->isInitializeSucceeded()) {
#else
    if (config->isInitializeSucceed()) {
#endif
        if (!config->enablePrelaunchSplash()) {
            skipCallback();
            return;
        }
        qCDebug(treelandCore) << "WindowConfigStore: configInitializeSucceeded for" << appId
                              << config->lastWindowWidth() << config->lastWindowHeight();
        const QSize size(static_cast<int>(config->lastWindowWidth()),
                         static_cast<int>(config->lastWindowHeight()));
        const QSize validatedSize = size.isValid() ? size : QSize();
        callback(validatedSize,
             config->splashDarkPalette(),
             config->splashLightPalette(),
             config->splashThemeType());
        return;
    }

    if (config->isInitializeFailed()) {
        qCWarning(treelandCore) << "WindowConfigStore: configInitializeFailed for" << appId;
        skipCallback();
        return;
    }

    waitCallback();
    auto *ctx = context ? context : const_cast<WindowConfigStore *>(this);
    const QPointer<AppConfig> configGuard(config);
    connect(
        config,
        &AppConfig::configInitializeSucceed,
        ctx,
        [this, appId, callback, skipCallback, configGuard](DTK_CORE_NAMESPACE::DConfig *) {
            if (!configGuard) {
                qCWarning(treelandCore)
                    << "WindowConfigStore: configInitializeSucceed but config deleted for" << appId;
                skipCallback();
                return;
            }

            if (!configGuard->enablePrelaunchSplash()) {
                skipCallback();
                return;
            }

            qCDebug(treelandCore) << "WindowConfigStore: configInitializeSucceed for" << appId
                                  << configGuard->lastWindowWidth()
                                  << configGuard->lastWindowHeight();
            const QSize size(static_cast<int>(configGuard->lastWindowWidth()),
                             static_cast<int>(configGuard->lastWindowHeight()));
            const QSize validatedSize = size.isValid() ? size : QSize();
            callback(validatedSize,
                     configGuard->splashDarkPalette(),
                     configGuard->splashLightPalette(),
                     configGuard->splashThemeType());
        },
        Qt::SingleShotConnection);
    connect(
        config,
        &AppConfig::configInitializeFailed,
        ctx,
        [skipCallback]() {
            qCCritical(treelandCore) << "WindowConfigStore: configInitializeFailed callback";
            skipCallback();
        },
        Qt::SingleShotConnection);
}
