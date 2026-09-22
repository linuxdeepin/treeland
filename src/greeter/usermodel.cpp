/***************************************************************************
 * Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
 * Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 ***************************************************************************/

#include "usermodel.h"

#include "common/treelandlogging.h"
#include "helper.h"
#include "session/session.h"

#include <Configuration.h>

#include <DDBusInterface>

#include <QFile>
#include <QGuiApplication>
#include <QList>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>
#include <QTranslator>

#include <memory>
#include <algorithm>
#include <pwd.h>
#include <unistd.h>

using namespace DDM;
DACCOUNTS_USE_NAMESPACE

struct UserModelPrivate
{
    bool containsAllUsers{ true };
    int lastIndex{ 0 };
    QString currentUserName;
    DAccountsManager manager;
    QTranslator *lastTrans{ nullptr };
    QList<UserPtr> users;
};

UserModel::UserModel(QObject *parent)
    : QAbstractListModel(parent)
    , d(new UserModelPrivate())
{
    connect(&d->manager, &DAccountsManager::UserAdded, this, &UserModel::onUserAdded);
    connect(&d->manager, &DAccountsManager::UserDeleted, this, &UserModel::onUserDeleted);

    auto userList = d->manager.userList();
    if (!userList) {
        qFatal() << userList.error();
    }

    const auto uids = userList.value();
    for (auto uid : uids) {
        auto user = d->manager.findUserById(uid);
        if (!user) {
            qCWarning(lcTlGreeter) << "Failed to find user by ID:" << user.error();
            continue;
        }

        addUser(std::make_unique<User>(std::move(user).value()));
    }

    // find out index of the last user
    auto lastUserName = stateConfig.Last.User.get();

    for (const auto &user : d->users) {
        if (user->userName() == lastUserName) {
            d->lastIndex = d->users.indexOf(user);
            d->currentUserName = user->userName();
            break;
        }
    }

    if (d->currentUserName.isEmpty()) {
        QString runningUser;
        uid_t runningUid = 0;
        if (const passwd *pw = getpwuid(getuid())) {
            runningUser = QString::fromLocal8Bit(pw->pw_name);
            runningUid = pw->pw_uid;
        }

        if (runningUser != QLatin1String("dde") && runningUid != 0) {
            UserPtr record = getUser(runningUid);
            if (!record && tryAddNssUser(runningUser)) {
                record = getUser(runningUid);
            }
            if (record) {
                d->currentUserName = record->userName();
                d->lastIndex = d->users.indexOf(record);
            }
        }
        if (d->currentUserName.isEmpty() && !d->users.isEmpty()) {
            d->currentUserName = d->users.first()->userName();
        }

        qCWarning(lcTlGreeter) << "No last user state, greeter starts with" << d->currentUserName;
    }
}

UserModel::~UserModel()
{
    delete d;
}

QHash<int, QByteArray> UserModel::roleNames() const
{
    // set role names
    QHash<int, QByteArray> names;
    names[NameRole] = QByteArrayLiteral("name");
    names[RealNameRole] = QByteArrayLiteral("realName");
    names[HomeDirRole] = QByteArrayLiteral("homeDir");
    names[IconRole] = QByteArrayLiteral("icon");
    names[NoPasswordRole] = QByteArrayLiteral("noPassword");
    names[LoggedInRole] = QByteArrayLiteral("loggedIn");
    names[IdentityRole] = QByteArrayLiteral("identity");
    names[PasswordHintRole] = QByteArrayLiteral("passwordHint");
    names[LocaleRole] = QByteArrayLiteral("locale");
    return names;
}

int UserModel::lastIndex() const
{
    return d->lastIndex;
}

QString UserModel::lastUser()
{
    return stateConfig.Last.User.get();
}

int UserModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(d->users.length());
}

bool UserModel::addUser(UserPtr user)
{
    const QString userName = user->userName();
    if (userName.isEmpty() || getUser(userName) || (user->UID() != 0 && getUser(user->UID()))) {
        qCInfo(lcTlGreeter) << "User" << userName << "already listed, merge richer data";
        UserPtr existing = getUser(userName);
        if (!existing) {
            existing = getUser(user->UID());
        }
        if (existing) {
            const int row = static_cast<int>(d->users.indexOf(existing));
            existing->merge(*user);
            Q_EMIT dataChanged(index(row), index(row));
        }
        return false;
    }

    beginResetModel();
    d->users.emplace_back(std::move(user));
    std::sort(d->users.begin(), d->users.end(), [](const UserPtr &u1, const UserPtr &u2) {
        return u1->userName() < u2->userName();
    });
    endResetModel();

    Q_EMIT countChanged();
    return true;
}

void UserModel::updateUserLoginState(const QString &username, bool loggedIn)
{
    // TODO: May remove once UserModel is guaranteed to resolve every loggable
    // user (consider whether users not manually added can still log in directly).
    UserPtr target = getUser(username);
    if (!target && loggedIn && tryAddNssUser(username)) {
        target = getUser(username);
    }
    if (!target) {
        if (const passwd *pw = ::getpwnam(username.toLocal8Bit().constData())) {
            target = getUser(static_cast<uid_t>(pw->pw_uid));
        }
    }
    if (!target && loggedIn) {
        qCWarning(lcTlGreeter) << "User" << username << "not found when updating login state";
    }

    if (target) {
        target->setLoggedIn(loggedIn);
        const int row = static_cast<int>(d->users.indexOf(target));
        Q_EMIT dataChanged(index(row), index(row));
    }

    Q_EMIT layoutChanged();
}

void UserModel::clearUserLoginState()
{
    for (auto &user : std::as_const(d->users)) {
        user->setLoggedIn(false);
    }

    Q_EMIT layoutChanged();
}

QVariant UserModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= d->users.count()) {
        return {};
    }

    // get user
    UserPtr user = d->users[index.row()];

    // return correct value
    switch (role) {
    case NameRole:
        return user->userName();
    case RealNameRole:
        return user->fullName();
    case HomeDirRole:
        return user->homeDir();
    case IconRole:
        return user->iconFile();
    case NoPasswordRole:
        return user->noPasswdLogin();
    case LoggedInRole:
        return user->loggedIn();
    case IdentityRole:
        return user->identity();
    case PasswordHintRole:
        return user->passwordHint();
    case LocaleRole:
        return user->locale();
    default:
        return {};
    }
}

bool UserModel::containsAllUsers() const
{
    return d->containsAllUsers;
}

QVariant UserModel::get(const QString &username) const
{
    QVariantMap map;
    for (const auto &user : d->users) {
        if (user->userName() == username or user->fullName() == username) {
            map["name"] = user->userName();
            map["icon"] = user->iconFile();
            map["realName"] = user->fullName();
            map["homeDir"] = user->homeDir();
            map["noPassword"] = user->noPasswdLogin();
            map["loggedIn"] = user->loggedIn();
            map["identity"] = user->identity();
            map["passwordHint"] = user->passwordHint();
            map["locale"] = user->locale();
            break;
        }
    }

    return map;
}

QVariant UserModel::get(int index) const
{
    QVariantMap map;
    if (index < 0 or index >= d->users.count()) {
        return {};
    }

    auto user = d->users.at(index);
    map["name"] = user->userName();
    map["icon"] = user->iconFile();
    map["realName"] = user->fullName();
    map["homeDir"] = user->homeDir();
    map["noPassword"] = user->noPasswdLogin();
    map["loggedIn"] = user->loggedIn();
    map["identity"] = user->identity();
    map["passwordHint"] = user->passwordHint();
    map["locale"] = user->locale();

    return map;
}

UserPtr UserModel::getUser(const QString &username) const noexcept
{
    for (const auto &user : d->users) {
        if (user->userName() == username) {
            return user;
        }
    }
    return nullptr;
}

UserPtr UserModel::getUser(uid_t uid) const noexcept
{
    for (const auto &user : d->users) {
        if (user->UID() == uid) {
            return user;
        }
    }
    return nullptr;
}

QString UserModel::currentUserName() const noexcept
{
    return d->currentUserName;
}

UserPtr UserModel::currentUser() const
{
    return getUser(d->currentUserName);
}

void UserModel::updateUserLimits(const QString &userName, const QString &time) const noexcept
{
    for (const auto &user : d->users) {
        if (user->userName() == userName) {
            user->updateLimitTime(time);
            break;
        }
    }
}

void UserModel::setCurrentUserName(const QString &userName) noexcept
{
    d->currentUserName = userName;

    for (const auto &user : d->users) {
        if (user->waylandSocket()) {
            user->waylandSocket()->setEnabled(user->userName() == userName,
                                              Helper::instance()->sessionManager()->globalSession()->socket());
        }
    }

    Q_EMIT currentUserNameChanged();
}

void UserModel::onUserAdded(quint64 uid)
{
    auto newUser = d->manager.findUserById(uid);
    if (!newUser) {
        qCWarning(lcTlGreeter) << "User" << uid << "has been added but couldn't find it.";
        return;
    }

    addUser(std::make_unique<User>(std::move(newUser).value()));
}

void UserModel::onUserDeleted(quint64 uid)
{
    beginResetModel();
    d->users.removeIf([uid](const UserPtr &user) {
        return user->UID() == uid;
    });
    endResetModel();

    Q_EMIT countChanged();
}

bool UserModel::tryAddNssUser(const QString &userName)
{
    if (userName.isEmpty()) {
        return false;
    }

    // Already in model?
    if (getUser(userName)) {
        qCInfo(lcTlGreeter) << "NSS user already in model:" << userName;
        return true;
    }

    // TODO: getpwnam is synchronous and may block on slow NSS/LDAP backends;
    // consider moving to an async worker thread in a future iteration.
    struct passwd *pw = ::getpwnam(userName.toLocal8Bit().constData());
    if (!pw) {
        qCInfo(lcTlGreeter) << "NSS user not found:" << userName;
        return false;
    }

    // pw_gecos is comma-separated ("Full Name,Room,Work,Home,Other"); use only the first field.
    QString fullName = QString::fromLocal8Bit(pw->pw_gecos).section(QLatin1Char(','), 0, 0);

    qCInfo(lcTlGreeter) << "Adding NSS/LDAP user to model:" << userName;
    return addUser(std::make_unique<User>(
        userName,
        static_cast<uid_t>(pw->pw_uid),
        static_cast<gid_t>(pw->pw_gid),
        QString::fromLocal8Bit(pw->pw_dir),
        fullName));
}
