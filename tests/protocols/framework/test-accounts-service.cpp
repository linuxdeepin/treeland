// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "test-accounts-service.h"

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QObject>

namespace {
constexpr auto accountsService = "org.freedesktop.Accounts";
constexpr auto accountsPath = "/org/freedesktop/Accounts";
constexpr auto accountsUserPath = "/org/freedesktop/Accounts/User1000";

class TestAccountsManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Accounts")
    Q_PROPERTY(QStringList UserList READ userList)

public:
    QStringList userList() const { return { QString::fromLatin1(accountsUserPath) }; }

public slots:
    QList<QDBusObjectPath> ListCachedUsers() const
    {
        return { QDBusObjectPath(QString::fromLatin1(accountsUserPath)) };
    }

    QDBusObjectPath FindUserById(qint64) const
    {
        return QDBusObjectPath(QString::fromLatin1(accountsUserPath));
    }
    QDBusObjectPath FindUserByName(const QString &) const
    {
        return QDBusObjectPath(QString::fromLatin1(accountsUserPath));
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
    auto connection = QDBusConnection::systemBus();
    constexpr auto exportOptions = QDBusConnection::ExportAllSlots
        | QDBusConnection::ExportAllProperties;
    return connection.isConnected()
        && connection.registerService(QString::fromLatin1(accountsService))
        && connection.registerObject(QString::fromLatin1(accountsPath), &d->manager, exportOptions)
        && connection.registerObject(QString::fromLatin1(accountsUserPath), &d->user, exportOptions);
}

#include "test-accounts-service.moc"
