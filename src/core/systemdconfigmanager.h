// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class AppConfig;
class OutputConfig;
class SeatUserDConfig;
class TreelandConfig;
class TreelandUserConfig;

class SystemDConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit SystemDConfigManager(QObject *parent = nullptr);
    ~SystemDConfigManager() override;

    static SystemDConfigManager *instance();

    bool isInitializeSucceeded() const;
    bool isInitializeFailed() const;

    TreelandConfig *globalConfig() const;
    TreelandUserConfig *userConfig(const QString &userName);
    SeatUserDConfig *seatUserConfig(const QString &userName);
    OutputConfig *outputConfig(const QString &outputName);
    AppConfig *appConfig(const QString &appId);

    // Creates and caches the initial user's configs. Returns true only when both
    // configs have already finished successfully; callers should listen for
    // their initialization signals when this returns false.
    bool initializeUserConfigs(const QString &userName);
    TreelandUserConfig *initialUserConfig() const;

Q_SIGNALS:
    void InitializeSucceed();
    void InitializeFailed();

private:
    static SystemDConfigManager *s_instance;

    TreelandConfig *m_globalConfig = nullptr;
    QHash<QString, TreelandUserConfig *> m_userConfigs;
    QHash<QString, SeatUserDConfig *> m_seatUserConfigs;
    QHash<QString, OutputConfig *> m_outputConfigs;
    QHash<QString, AppConfig *> m_appConfigs;
    TreelandUserConfig *m_initialUserConfig = nullptr;
};
