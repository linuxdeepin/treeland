// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
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

    QSize lastSizeFor(const QString &appId) const;
    void withLastSizeFor(const QString &appId,
                         QObject *context,
                         std::function<void(const QSize &size)> callback) const;
    void saveSize(const QString &appId, const QSize &size);

    // Per-app theme preference: 0 follow system, 1 dark, 2 light
    qlonglong themeTypeFor(const QString &appId) const;
    void setThemeType(const QString &appId, qlonglong themeType);

private:
    AppConfig *configForApp(const QString &appId) const;

    mutable QHash<QString, AppConfig *> m_appConfigs;
};