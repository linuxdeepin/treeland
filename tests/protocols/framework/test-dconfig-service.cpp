// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "test-dconfig-service.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>

#include <csignal>
#include <cerrno>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
constexpr auto dconfigService = "org.desktopspec.ConfigManager";

// SIGKILL leaves the bus socket behind: only a graceful shutdown unlinks it.
// A child that outlives its parent would also stay resident until it is reaped
// by init, so make every helper die with this process.
void exitWithParent(pid_t parent)
{
    prctl(PR_SET_PDEATHSIG, SIGTERM);
    // The parent may already be gone; the handler would then belong to the
    // process that adopted us and never fire for the parent we care about.
    if (getppid() != parent)
        _exit(0);
}

// A helper leads its own process group, which then also covers services that
// D-Bus activates on its bus: they are reparented to a babysitter, so an
// exit-time PPID walk misses them while the process group still holds them.
// Without this the helper would inherit the test's group, which also holds
// ctest, so the group could never be signalled as a unit.
void leadOwnProcessGroup()
{
    setpgid(0, 0);
}

void stopProcess(pid_t &pid)
{
    if (pid <= 0)
        return;

    kill(-pid, SIGTERM);
    for (int attempt = 0; attempt != 100; ++attempt) {
        if (waitpid(pid, nullptr, WNOHANG) == pid) {
            // Activated services are not our children and cannot be reaped
            // here; one group-wide SIGKILL cleans up any stragglers.
            kill(-pid, SIGKILL);
            pid = -1;
            return;
        }
        usleep(10'000);
    }

    kill(-pid, SIGKILL);
    while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {
    }
    pid = -1;
}
}

bool TestDConfigService::start()
{
    const pid_t parent = getpid();

    int addressPipe[2];
    if (pipe(addressPipe) != 0)
        return false;

    // A Linux abstract socket lives only in the kernel: unlike a /tmp socket it
    // cannot be left behind when this process is killed by CTest.
    static int instance = 0;
    const QByteArray address = "unix:abstract=treeland-protocol-test-"
        + QByteArray::number(parent) + '-' + QByteArray::number(++instance);
    const QByteArray addressArgument = "--address=" + address;

    m_busPid = fork();
    if (m_busPid == 0) {
        leadOwnProcessGroup();
        exitWithParent(parent);
        close(addressPipe[0]);
        dup2(addressPipe[1], STDOUT_FILENO);
        close(addressPipe[1]);
        execlp("dbus-daemon",
               "dbus-daemon",
               "--session",
               "--nofork",
               "--print-address=1",
               "--nopidfile",
               addressArgument.constData(),
               nullptr);
        _exit(127);
    }
    close(addressPipe[1]);
    if (m_busPid < 0) {
        close(addressPipe[0]);
        return false;
    }
    // Close the window where the parent signals the group before the child
    // has moved itself into it.
    setpgid(m_busPid, m_busPid);

    QByteArray printedAddress;
    char character;
    while (printedAddress.size() < 4096 && read(addressPipe[0], &character, 1) == 1) {
        if (character == '\n')
            break;
        printedAddress.append(character);
    }
    close(addressPipe[0]);
    if (printedAddress.isEmpty()) {
        stop();
        return false;
    }
    qputenv("DBUS_SYSTEM_BUS_ADDRESS", printedAddress);
    qputenv("DBUS_SESSION_BUS_ADDRESS", printedAddress);
    return startDConfigService();
}

bool TestDConfigService::waitForService()
{
    const auto connection = QDBusConnection::systemBus();
    return connection.isConnected() && waitForService(connection, dconfigService);
}

void TestDConfigService::stop()
{
    // The helpers lead their own process groups, so stopping each group also
    // takes down any service the bus activated.
    stopProcess(m_dconfigPid);
    stopProcess(m_busPid);
    if (!m_dconfigPrefix.isEmpty())
        QDir(m_dconfigPrefix).removeRecursively();
}

bool TestDConfigService::startDConfigService()
{
    const pid_t parent = getpid();

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
        leadOwnProcessGroup();
        exitWithParent(parent);
        execlp("dde-dconfig-daemon",
               "dde-dconfig-daemon",
               "-p",
               prefix,
               nullptr);
        _exit(127);
    }
    if (m_dconfigPid > 0)
        setpgid(m_dconfigPid, m_dconfigPid);
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
