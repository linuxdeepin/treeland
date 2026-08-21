// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QString>

#include <sys/types.h>

class QDBusConnection;

class TestDConfigService
{
public:
    bool start();
    bool waitForService();
    void stop();

private:
    bool startDConfigService();
    static bool waitForService(const QDBusConnection &connection, const QString &service);

    pid_t m_busPid = -1;
    pid_t m_dconfigPid = -1;
    QString m_dconfigPrefix;
};
