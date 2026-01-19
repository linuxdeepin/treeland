// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QObject>
#include <QSize>
#include <QHash>
#include <QString>

class AppConfig;

// Simple window size persistence backed by dconfig
class WindowSizeStore : public QObject {
    Q_OBJECT
public:
    explicit WindowSizeStore(QObject *parent = nullptr);

    QSize lastSizeFor(const QString &appId) const;
    void saveSize(const QString &appId, const QSize &size);

    // Per-app theme preference: 0 follow system, 1 dark, 2 light
    qlonglong themeTypeFor(const QString &appId) const;
    void setThemeType(const QString &appId, qlonglong themeType);

private:
    AppConfig *configForApp(const QString &appId) const;

    mutable QHash<QString, AppConfig *> m_appConfigs;
};
