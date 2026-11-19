// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "test-accounts-service.h"

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QObject>

#include <pwd.h>
#include <unistd.h>

namespace {
constexpr auto accountsService = "org.freedesktop.Accounts";
constexpr auto accountsPath = "/org/freedesktop/Accounts";

class TestAccountsManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Accounts")
    Q_PROPERTY(QStringList UserList READ userList)

public:
    QString userPath = QStringLiteral("/org/freedesktop/Accounts/User1000");
    QStringList userList() const { return { userPath }; }

public Q_SLOTS:
    QList<QDBusObjectPath> ListCachedUsers() const
    {
        return { QDBusObjectPath(userPath) };
    }

    QDBusObjectPath FindUserById(qint64) const
    {
        return QDBusObjectPath(userPath);
    }
    QDBusObjectPath FindUserByName(const QString &) const
    {
        return QDBusObjectPath(userPath);
    }
};

class TestAccountsUser : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Accounts.User")
    Q_PROPERTY(int AccountType MEMBER accountType)
    Q_PROPERTY(bool NoPasswdLogin MEMBER noPasswdLogin)
    Q_PROPERTY(qulonglong Uid MEMBER uid)
    Q_PROPERTY(qulonglong Gid MEMBER gid)
    Q_PROPERTY(QString UserName MEMBER userName)
    Q_PROPERTY(QString FullName MEMBER fullName)
    Q_PROPERTY(QString HomeDir MEMBER homeDir)
    Q_PROPERTY(QString IconFile MEMBER iconFile)
    Q_PROPERTY(QString PasswordHint MEMBER passwordHint)
    Q_PROPERTY(QString Locale MEMBER locale)

public:
    int accountType = 0;
    bool noPasswdLogin = false;
    qulonglong uid = 1000;
    qulonglong gid = 1000;
    QString userName = QStringLiteral("protocol-test");
    QString fullName = QStringLiteral("Protocol Test");
    QString homeDir = QStringLiteral("/tmp");
    QString iconFile;
    QString passwordHint;
    QString locale = QStringLiteral("C");
};
}

class TestAccountsService::Private
{
public:
    TestAccountsManager manager;
    TestAccountsUser user;
};

TestAccountsService::TestAccountsService()
    : d(std::make_unique<Private>())
{
}

TestAccountsService::~TestAccountsService() = default;

bool TestAccountsService::registerObjects()
{
    if (qEnvironmentVariableIsSet("TREELAND_PROTOCOL_TEST_USE_RUNNING_ACCOUNT")) {
        const auto *account = getpwuid(getuid());
        if (!account)
            return false;
        d->user.uid = account->pw_uid;
        d->user.gid = account->pw_gid;
        d->user.userName = QString::fromLocal8Bit(account->pw_name);
        d->user.homeDir = QString::fromLocal8Bit(account->pw_dir);
        d->manager.userPath = QStringLiteral("/org/freedesktop/Accounts/User%1").arg(d->user.uid);
    }
    auto connection = QDBusConnection::systemBus();
    constexpr auto exportOptions = QDBusConnection::ExportAllSlots
        | QDBusConnection::ExportAllProperties;
    return connection.isConnected()
        && connection.registerService(QString::fromLatin1(accountsService))
        && connection.registerObject(QString::fromLatin1(accountsPath), &d->manager, exportOptions)
        && connection.registerObject(d->manager.userPath, &d->user, exportOptions);
}

#include "test-accounts-service.moc"
