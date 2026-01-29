// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QHash>
#include <QObject>
#include <QSize>
#include <QString>

#include <functional>

class AppConfig;

// Window configuration persistence backed by dconfig
class WindowConfigStore : public QObject
{
    Q_OBJECT
public:
    explicit WindowConfigStore(QObject *parent = nullptr);

    void saveLastSize(const QString &appId, const QSize &size);

    void withSplashConfigFor(
        const QString &appId,
        QObject *context,
        std::function<void(const QSize &size,
                           const QString &darkPalette,
                           const QString &lightPalette,
                           qlonglong splashThemeType)> callback,
        std::function<void()> skipCallback,
        std::function<void()> waitCallback) const;

private:
    AppConfig *configForApp(const QString &appId) const;

    mutable QHash<QString, AppConfig *> m_appConfigs;
};
