// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QString>
#include <memory>

class TreelandConfig;
class TreelandUserConfig;

class SystemDConfigManager : public QObject
{
    Q_OBJECT
public:
    explicit SystemDConfigManager(QObject *parent = nullptr);
    ~SystemDConfigManager() override;

    // Block until the global config finishes initializing (success, failure,
    // or timeout). Call once after construction, before the global config is
    // consumed. Splitting the wait out of the constructor lets Helper assign the
    // manager pointer first, so any nested-loop callback reaching
    // Helper::config()/globalConfig() sees a valid pointer instead of null.
    void initialize();

    // True only when both configs finished initializing *successfully*. A
    // failure or timeout is intentionally not counted as success, so the
    // initializeSucceed signal is not emitted for degraded configs. In the
    // greeter ("dde" placeholder) scenario the user config is deliberately not
    // awaited, so this stays false by design and the signal does not fire.
    bool isInitializeSucceeded() const;

    TreelandConfig *globalConfig() const;
    TreelandUserConfig *userConfig() const;

    // Rebuild the user-scoped config for the given user and block until it
    // finishes initializing (real user only; the "dde" placeholder is not
    // awaited). See isInitializeSucceeded() for the greeter contract.
    void resetUserConfig(const QString &userName);

Q_SIGNALS:
    void initializeSucceed();

private:
    // Returns true when the config initialized successfully, false on failure
    // or timeout (defaults are then in effect, which is still safe to read).
    bool waitForGlobalConfigInitialized();
    bool waitForUserConfigInitialized();
    void checkInitialized();

    std::unique_ptr<TreelandConfig> m_globalConfig;
    std::unique_ptr<TreelandUserConfig> m_userConfig;
    bool m_globalConfigInitialized = false;
    bool m_userConfigInitialized = false;
    bool m_initializeSucceedEmitted = false;
};
