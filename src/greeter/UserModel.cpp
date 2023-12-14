/***************************************************************************
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

#include "UserModel.h"
#include "Constants.h"
#include "Configuration.h"
#include <QFile>
#include <QList>
#include <QTextStream>
#include <QStringList>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusConnection>
#include <memory>
#include <pwd.h>

using namespace SDDM;

struct User {
    User(const struct passwd *data, const QString& icon) :
        needsPassword(strcmp(data->pw_passwd, "") != 0),
        uid(static_cast<int>(data->pw_uid)),
        gid(static_cast<int>(data->pw_gid)),
        name(QString::fromLocal8Bit(data->pw_name)),
        realName(QString::fromLocal8Bit(data->pw_gecos).split(QLatin1Char(',')).first()),
        homeDir(QString::fromLocal8Bit(data->pw_dir)),
        icon(icon)
    {
        auto msg = QDBusMessage::createMethodCall("org.freedesktop.Accounts",QString{"/org/freedesktop/Accounts/User%1"}.arg(uid),"org.freedesktop.DBus.Properties","Get");
        msg.setArguments({QString{"org.freedesktop.Accounts.User"}, QString{"AccountType"}});

        auto reply = QDBusConnection::systemBus().call(msg);
        if(reply.type() != QDBusMessage::ReplyMessage){
            qWarning() << "get user " << uid << "identity failed:" << reply.errorMessage();
            return;
        }

        auto type = reply.arguments().first().value<QDBusVariant>().variant().toInt();
        switch (type){
        case 0:
            identity = QString{"Standard User"};
            return;
        case 1:
            identity = QString{"Administrator"};
            return;
        default:
            qDebug() << "ignore others type";
            return;
        }
    }

    bool needsPassword {false};
    bool logined {false};
    int uid {0};
    int gid {0};
    QString name;
    QString realName;
    QString homeDir;
    QString icon;
    QString identity;
};

using UserPtr = std::shared_ptr<User>;

struct UserModelPrivate {
    bool containsAllUsers {true};
    int lastIndex {0};
    QList<UserPtr> users;
};

UserModel::UserModel(bool needAllUsers, QObject *parent) :
    QAbstractListModel(parent), d(new UserModelPrivate()) {
    const QString facesDir = mainConfig.Theme.FacesDir.get();
    const QString themeDir = mainConfig.Theme.ThemeDir.get();
    const QString currentTheme = mainConfig.Theme.Current.get();
    const QString themeDefaultFace = QStringLiteral("%1/%2/faces/.face.icon").arg(themeDir, currentTheme);
    const QString defaultFace = QStringLiteral("%1/.face.icon").arg(facesDir);
    const QString iconURI = QStringLiteral("file://%1").arg(
        QFile::exists(themeDefaultFace) ? themeDefaultFace : defaultFace);

    bool lastUserFound = false;

    struct passwd *current_pw;
    setpwent();
    while ((current_pw = getpwent()) != nullptr) {

        // skip entries with uids smaller than minimum uid
        if (int(current_pw->pw_uid) < mainConfig.Users.MinimumUid.get())
            continue;

        // skip entries with uids greater than maximum uid
        if (int(current_pw->pw_uid) > mainConfig.Users.MaximumUid.get())
            continue;
        // skip entries with user names in the hide users list
        if (mainConfig.Users.HideUsers.get().contains(QString::fromLocal8Bit(current_pw->pw_name)))
            continue;

        // skip entries with shells in the hide shells list
        if (mainConfig.Users.HideShells.get().contains(QString::fromLocal8Bit(current_pw->pw_shell)))
            continue;

        // create user
        auto user = std::make_shared<User>(current_pw, iconURI);

        if (user->name == stateConfig.Last.User.get())
            lastUserFound = true;

        // add user
        d->users.emplaceBack(std::move(user));

        if (!needAllUsers && d->users.count() > mainConfig.Theme.DisableAvatarsThreshold.get()) {
            struct passwd *lastUserData;
            // If the theme doesn't require that all users are present, try to add the data for lastUser at least
            if(!lastUserFound && (lastUserData = getpwnam(qPrintable(lastUser()))))
                d->users.emplaceBack(std::make_shared<User>(lastUserData, themeDefaultFace));

            d->containsAllUsers = false;
            break;
        }
    }

    endpwent();

    // sort users by username
    std::sort(d->users.begin(), d->users.end(), [](const UserPtr &u1, const UserPtr &u2) { return u1->name < u2->name; });
    // Remove duplicates in case we have several sources specified
    // in nsswitch.conf(5).
    auto newEnd = std::unique(d->users.begin(), d->users.end(), [](const UserPtr &u1, const UserPtr &u2) { return u1->name == u2->name; });
    d->users.erase(newEnd, d->users.end());

    bool avatarsEnabled = mainConfig.Theme.EnableAvatars.get();
    if (avatarsEnabled && mainConfig.Theme.EnableAvatars.isDefault()) {
        if (d->users.count() > mainConfig.Theme.DisableAvatarsThreshold.get())
            avatarsEnabled=false;
    }

    // find out index of the last user
    auto luser = stateConfig.Last.User.get();
    for (int i = 0; i < d->users.size(); ++i) {
        UserPtr user { d->users.at(i) };
        if (user->name == luser)
            d->lastIndex = i;

        if (avatarsEnabled) {
            const QString userFace = QStringLiteral("%1/.face.icon").arg(user->homeDir);
            const QString systemFace = QStringLiteral("%1/%2.face.icon").arg(facesDir, user->name);
            const QString accountsServiceFace = QStringLiteral(ACCOUNTSSERVICE_DATA_DIR "/icons/%1").arg(user->name);

            QString userIcon;
            // If the home is encrypted it takes a lot of time to open
            // up the greeter, therefore we try the system avatar first
            if (QFile::exists(systemFace))
                userIcon = systemFace;
            else if (QFile::exists(userFace))
                userIcon = userFace;
            else if (QFile::exists(accountsServiceFace))
                userIcon = accountsServiceFace;

            if (!userIcon.isEmpty())
                user->icon = QStringLiteral("file://%1").arg(userIcon);
        }
    }
}

UserModel::~UserModel() {
    delete d;
}

QHash<int, QByteArray> UserModel::roleNames() const {
    // set role names
    static auto roleNames = [](){
        QHash<int, QByteArray> names;
        names[NameRole] = QByteArrayLiteral("name");
        names[RealNameRole] = QByteArrayLiteral("realName");
        names[HomeDirRole] = QByteArrayLiteral("homeDir");
        names[IconRole] = QByteArrayLiteral("icon");
        names[NeedsPasswordRole] = QByteArrayLiteral("needsPassword");
        names[LoginedRole] = QByteArray("logined");
        names[IdentityRole] = QByteArray("identity");
        return names;
    }();

    return roleNames;
}

int UserModel::lastIndex() const {
    return d->lastIndex;
}

QString UserModel::lastUser() {
    return stateConfig.Last.User.get();
}

int UserModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : static_cast<int>(d->users.length());
}

void UserModel::updateUserLoginState(const QString &username, bool logined)
{
    for (auto it = d->users.begin(); it != d->users.end(); ++it) {
        if (it->get()->name == username) {
            it->get()->logined = logined;
            auto pos = d->users.end() - it;
            emit dataChanged(index(0, pos - 1), index(0, pos));
            break;
        }
    }
    emit layoutChanged();
}

QVariant UserModel::data(const QModelIndex &index, int role) const {
    if (index.row() < 0 || index.row() > d->users.count())
        return {};

    // get user
    UserPtr user = d->users[index.row()];

    // return correct value
    switch(role) {
        case NameRole:
            return user->name;
        case RealNameRole:
            return user->realName;
        case HomeDirRole:
            return user->homeDir;
        case IconRole:
            return user->icon;
        case NeedsPasswordRole:
            return user->needsPassword;
        case LoginedRole:
            return user->logined;
        case IdentityRole:
            return user->identity;
        default:
            return {};
    }

    Q_UNREACHABLE();
}

int UserModel::disableAvatarsThreshold() {
    return mainConfig.Theme.DisableAvatarsThreshold.get();
}

bool UserModel::containsAllUsers() const {
    return d->containsAllUsers;
}

QVariant UserModel::get(const QString &username) const {
    QVariantMap map;
    for (const auto& user : d->users) {
        if (user->name == username or user->realName == username) {
            map["name"] = user->name;
            map["icon"] = user->icon;
            map["realName"] = user->realName;
            map["homeDir"] = user->homeDir;
            map["needsPassword"] = user->needsPassword;
            map["logined"] = user->logined;
            map["identity"] = user->identity;
            break;
        }
    }

    return map;
}

QVariant UserModel::get(int index) const {
    QVariantMap map;
    if(index < 0 or index > d->users.count()){
        return {};
    }

    auto user = d->users.at(index);
    map["name"] = user->name;
    map["icon"] = user->icon;
    map["realName"] = user->realName;
    map["homeDir"] = user->homeDir;
    map["needsPassword"] = user->needsPassword;
    map["logined"] = user->logined;
    map["identity"] = user->identity;

    return map;
}
