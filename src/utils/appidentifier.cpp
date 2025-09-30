// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "appidentifier.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QLoggingCategory>
#include <QString>
#include <QVariant>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(treelandAppIdentifier, "treeland.appidentifier", QtInfoMsg)

namespace {

constexpr const char *DDEApplicationManager1ServiceName = "org.desktopspec.ApplicationManager1";
constexpr const char *DDEApplicationManager1ObjectPath = "/org/desktopspec/ApplicationManager1";
constexpr const char *ApplicationManager1Interface = "org.desktopspec.ApplicationManager1";

static QString identifyWithRawPidfd(int pidfd, const char *hint)
{
    if (pidfd < 0) return QString();
    int dupfd = fcntl(pidfd, F_DUPFD_CLOEXEC, 0);
    if (dupfd == -1) {
        qCWarning(treelandAppIdentifier) << "fcntl dup pidfd failed" << hint << strerror(errno);
        return QString();
    }
    QDBusUnixFileDescriptor wrapper(dupfd); // QDBusUnixFileDescriptor 会再 dup，一旦构造成功即可关闭 dupfd
    close(dupfd);
    auto msg = QDBusMessage::createMethodCall(DDEApplicationManager1ServiceName,
                                              DDEApplicationManager1ObjectPath,
                                              ApplicationManager1Interface,
                                              QStringLiteral("Identify"));
    msg << QVariant::fromValue(wrapper);
    auto reply = QDBusConnection::sessionBus().call(msg, QDBus::BlockWithGui);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
        qCWarning(treelandAppIdentifier) << "Identify call failed" << hint << reply.errorMessage();
        return QString();
    }
    QString appId = reply.arguments().first().toString();
    if (appId.isEmpty()) {
        qCWarning(treelandAppIdentifier) << "Empty appId" << hint;
        return QString();
    }
    return appId;
}
} // unnamed

namespace Treeland::Utils {

QString appIdFromPidfd(int pidfd)
{
    if (pidfd < 0)
        return QString();
    return identifyWithRawPidfd(pidfd, "(pidfd)");
}

} // namespace Treeland::Utils
