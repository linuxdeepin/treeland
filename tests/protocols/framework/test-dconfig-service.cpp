// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "test-dconfig-service.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

namespace {
constexpr auto dconfigService = "org.desktopspec.ConfigManager";
}

bool TestDConfigService::start()
{
    int addressPipe[2];
    if (pipe(addressPipe) != 0)
        return false;

    m_busPid = fork();
    if (m_busPid == 0) {
        close(addressPipe[0]);
        dup2(addressPipe[1], STDOUT_FILENO);
        close(addressPipe[1]);
        execlp("dbus-daemon",
               "dbus-daemon",
               "--session",
               "--nofork",
               "--print-address=1",
               "--nopidfile",
               nullptr);
        _exit(127);
    }
    close(addressPipe[1]);
    if (m_busPid < 0) {
        close(addressPipe[0]);
        return false;
    }

    QByteArray address;
    char character;
    while (address.size() < 4096 && read(addressPipe[0], &character, 1) == 1) {
        if (character == '\n')
            break;
        address.append(character);
    }
    close(addressPipe[0]);
    if (address.isEmpty()) {
        stop();
        return false;
    }
    qputenv("DBUS_SYSTEM_BUS_ADDRESS", address);
    qputenv("DBUS_SESSION_BUS_ADDRESS", address);
    return startDConfigService();
}

bool TestDConfigService::waitForService()
{
    const auto connection = QDBusConnection::systemBus();
    return connection.isConnected() && waitForService(connection, dconfigService);
}

void TestDConfigService::stop()
{
    if (m_dconfigPid > 0) {
        kill(m_dconfigPid, SIGTERM);
        waitpid(m_dconfigPid, nullptr, 0);
        m_dconfigPid = -1;
    }
    if (m_busPid > 0) {
        kill(m_busPid, SIGTERM);
        waitpid(m_busPid, nullptr, 0);
        m_busPid = -1;
    }
    if (!m_dconfigPrefix.isEmpty())
        QDir(m_dconfigPrefix).removeRecursively();
}

bool TestDConfigService::startDConfigService()
{
    char prefix[] = "/tmp/treeland-protocol-dconfig-XXXXXX";
    if (!mkdtemp(prefix))
        return false;
    m_dconfigPrefix = QString::fromLocal8Bit(prefix);
    const QByteArray dsgDir = qgetenv("TREELAND_PROTOCOL_TEST_DSG_DIR");
    if (dsgDir.isEmpty() || !QDir().mkpath(m_dconfigPrefix + "/usr/share"))
        return false;
    const QByteArray dsgLink = (m_dconfigPrefix + "/usr/share/dsg").toLocal8Bit();
    if (symlink(dsgDir.constData(), dsgLink.constData()) != 0)
        return false;

    m_dconfigPid = fork();
    if (m_dconfigPid == 0) {
        execlp("dde-dconfig-daemon",
               "dde-dconfig-daemon",
               "-p",
               prefix,
               nullptr);
        _exit(127);
    }
    return m_dconfigPid > 0;
}

bool TestDConfigService::waitForService(const QDBusConnection &connection,
                                                 const QString &service)
{
    auto *interface = connection.interface();
    if (!interface)
        return false;
    for (int attempt = 0; attempt != 100; ++attempt) {
        const auto registered = interface->isServiceRegistered(service);
        if (registered.isValid() && registered.value())
            return true;
        usleep(10'000);
    }
    return false;
}
