// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "user.h"

#include "common/treelandlogging.h"

#include <QObject>
#include <QUrl>

struct UserPrivate
{
    bool loggedIn{ false };
    bool noPasswdLogin{ false };
    quint64 uid{ 0 };
    quint64 gid{ 0 };
    QString userName;
    QString fullName;
    QString homeDir;
    QUrl icon;
    QLocale locale;
    AccountTypes identity;
    QString passwordHint;
    QString limitTime;
    AccountsUserPtr inter{ nullptr };
    std::shared_ptr<WAYLIB_SERVER_NAMESPACE::WSocket> waylandSocket;

    void updateUserData();
};

void UserPrivate::updateUserData()
{
    noPasswdLogin = inter->noPasswdLogin();
    identity = inter->accountType();
    uid = inter->UID();
    gid = inter->GID();
    userName = inter->userName();
    fullName = inter->fullName();
    homeDir = inter->homeDir();
    icon = inter->iconFile();
    passwordHint = inter->passwordHint();
    locale = QLocale{ inter->locale() };
}

User::User(AccountsUserPtr ptr)
    : d(new UserPrivate{})
{
    d->inter = std::move(ptr);

    if (!d->inter) {
        qCFatal(treelandGreeter) << "connect to AccountService Failed";
    }

    connect(d->inter.data(), &DAccountsUser::userDataChanged, [this] {
        d->updateUserData();
        Q_EMIT userDataChanged();
    });

    d->updateUserData();
}

User::~User()
{
    delete d;
}

bool User::noPasswdLogin() const noexcept
{
    return d->noPasswdLogin;
}

QString User::identity() const noexcept
{
    return toString(d->identity);
}

quint64 User::UID() const noexcept
{
    return d->uid;
}

quint64 User::GID() const noexcept
{
    return d->gid;
}

const QString &User::userName() const noexcept
{
    return d->userName;
}

const QString &User::fullName() const noexcept
{
    return d->fullName;
}

const QString &User::homeDir() const noexcept
{
    return d->homeDir;
}

const QUrl &User::iconFile() const noexcept
{
    return d->icon;
}

const QString &User::passwordHint() const noexcept
{
    return d->passwordHint;
}

const QLocale &User::locale() const noexcept
{
    return d->locale;
}

bool User::loggedIn() const noexcept
{
    return d->loggedIn;
}

void User::setLoggedIn(bool newState) const noexcept
{
    d->loggedIn = newState;
}

void User::updateLimitTime(const QString &time) noexcept
{
    d->limitTime = time;
    Q_EMIT limitTimeChanged(time);
}

void User::setWaylandSocket(std::shared_ptr<WAYLIB_SERVER_NAMESPACE::WSocket> socket)
{
    d->waylandSocket = socket;
}

std::shared_ptr<WAYLIB_SERVER_NAMESPACE::WSocket> User::waylandSocket() const
{
    return d->waylandSocket;
}

QString User::toString(AccountTypes type) noexcept
{
    DACCOUNTS_USE_NAMESPACE
    switch (type) {
    case AccountTypes::Admin:
        return tr("Administrator");
    case AccountTypes::Default:
        return tr("Standard User");
    default:
        qCWarning(treelandGreeter) << "ignore other types.";
    }

    return {};
}
