// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "core/windowsizestore.h"

#include "common/treelandlogging.h"
#include "appconfig.hpp"

WindowSizeStore::WindowSizeStore(QObject *parent)
    : QObject(parent)
{
}

AppConfig *WindowSizeStore::configForApp(const QString &appId) const
{
    if (appId.isEmpty()) {
        return nullptr;
    }

    if (auto *config = m_appConfigs.value(appId)) {
        return config;
    }

    auto *config = AppConfig::create(QStringLiteral("org.deepin.dde.treeland"), appId, const_cast<WindowSizeStore *>(this));
    m_appConfigs.insert(appId, config);

    connect(config, &AppConfig::configInitializeFailed, this, [appId](DTK_CORE_NAMESPACE::DConfig *backend) {
        qCWarning(treelandCore) << "WindowSizeStore: dconfig init failed for app" << appId << backend;
    });

    return config;
}

QSize WindowSizeStore::lastSizeFor(const QString &appId) const
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

    qCDebug(treelandCore) << "WindowSizeStore: last size for" << appId << "is" << size;
    return size;
}

void WindowSizeStore::saveSize(const QString &appId, const QSize &size)
{
    if (appId.isEmpty() || !size.isValid()) {
        return;
    }

    auto *config = configForApp(appId);
    if (!config) {
        return;
    }

    qCInfo(treelandCore) << "WindowSizeStore: save size for" << appId << "as" << size;
    config->setLastWindowWidth(size.width());
    config->setLastWindowHeight(size.height());
}
