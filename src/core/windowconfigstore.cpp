// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/windowconfigstore.h"

#include "common/treelandlogging.h"
#include "appconfig.hpp"

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

    connect(config, &AppConfig::configInitializeFailed, this, [appId]() {
        qCWarning(treelandCore) << "WindowConfigStore: dconfig init failed for app" << appId;
    });
    connect(config,
            &AppConfig::valueChanged,
            this,
            [appId](const QString &key, const QVariant &value) {
                qCInfo(treelandCore)
                    << "WindowConfigStore: valueChanged app" << appId << "key" << key
                    << "value" << value;
            });

    return config;
}

QSize WindowConfigStore::lastSizeFor(const QString &appId) const
{
    auto *config = configForApp(appId);
    if (!config) {
        return {};
    }

    const QSize size(static_cast<int>(config->lastWindowWidth()),
                     static_cast<int>(config->lastWindowHeight()));

    if (!size.isValid()) {
        return {};
    }

    qCDebug(treelandCore) << "WindowConfigStore: last size for" << appId << "is" << size;
    return size;
}

void WindowConfigStore::withLastSizeFor(const QString &appId,
                                        QObject *context,
                                        std::function<void(const QSize &size,
                                                           qlonglong themeType,
                                                           bool prelaunchSplashEnabled)> callback) const
{
    if (!callback) {
        return;
    }

    if (appId.isEmpty()) {
        callback({}, 0, false);
        return;
    }
    qCDebug(treelandCore) << "WindowConfigStore: withLastSizeFor requested for" << appId;

    auto *config = configForApp(appId);
    if (!config) {
        callback({}, 0, false);
        return;
    }

    if (config->isInitializeSucceeded()) {
        qCDebug(treelandCore) << "WindowConfigStore: configInitializeSucceeded for" << appId
                     << config->lastWindowWidth() << config->lastWindowHeight();
        callback(lastSizeFor(appId), themeTypeFor(appId), prelaunchSplashEnabledFor(appId));
        return;
    }

    if (config->isInitializeFailed()) {
        qCWarning(treelandCore) << "WindowConfigStore: configInitializeFailed for" << appId;
        callback({}, 0, false);
        return;
    }

    auto *ctx = context ? context : const_cast<WindowConfigStore *>(this);
    connect(config,
            &AppConfig::configInitializeSucceed,
            ctx,
            [this, appId, callback, config](DTK_CORE_NAMESPACE::DConfig *) {
                qCDebug(treelandCore) << "WindowConfigStore: configInitializeSucceed for" << appId
                             << config->lastWindowWidth() << config->lastWindowHeight();

                callback(lastSizeFor(appId),
                         themeTypeFor(appId),
                         prelaunchSplashEnabledFor(appId));
            },
            Qt::SingleShotConnection);
    connect(config,
            &AppConfig::configInitializeFailed,
            ctx,
            [callback]() {
                qCWarning(treelandCore) << "WindowConfigStore: configInitializeFailed callback";
                callback({}, 0, false);
            },
            Qt::SingleShotConnection);
}

void WindowConfigStore::saveSize(const QString &appId, const QSize &size)
{
    if (appId.isEmpty() || !size.isValid()) {
        return;
    }

    auto *config = configForApp(appId);
    if (!config) {
        return;
    }

    qCDebug(treelandCore) << "WindowConfigStore: save size for" << appId << "as" << size;
    config->setLastWindowWidth(size.width());
    config->setLastWindowHeight(size.height());
}

qlonglong WindowConfigStore::themeTypeFor(const QString &appId) const
{
    auto *config = configForApp(appId);
    if (!config) {
        return 0; // follow system by default
    }
    return config->themeType();
}

void WindowConfigStore::setThemeType(const QString &appId, qlonglong themeType)
{
    if (appId.isEmpty()) {
        return;
    }
    auto *config = configForApp(appId);
    if (!config) {
        return;
    }
    qCDebug(treelandCore) << "WindowConfigStore: set themeType for" << appId << "as" << themeType;
    config->setThemeType(themeType);
}

bool WindowConfigStore::prelaunchSplashEnabledFor(const QString &appId) const
{
    auto *config = configForApp(appId);
    if (!config) {
        return false;
    }
    return config->enablePrelaunchSplash();
}
