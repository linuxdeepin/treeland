// SPDX-FileCopyrightText: 2021 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "userinfo.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <grp.h>
#include <memory>
#include <pwd.h>
#include <unistd.h>

#define DEFAULT_AVATAR ":/img/default_avatar.svg"
#define DEFAULT_BACKGROUND "/usr/share/backgrounds/default_background.jpg"

User::User(QObject *parent)
    : QObject(parent)
    , m_isAutomaticLogin(false)
    , m_isLogin(false)
    , m_isNoPasswordLogin(false)
    , m_isPasswordValid(true)
    , m_isUse24HourFormat(true)
    , m_expiredDayLeft(0)
    , m_expiredState(ExpiredNormal)
    , m_lastAuthType(0)
    , m_shortDateFormat(0)
    , m_shortTimeFormat(0)
    , m_weekdayFormat(0)
    , m_accountType(-1)
    , m_uid(INT_MAX)
    , m_avatar(DEFAULT_AVATAR)
    , m_greeterBackground(DEFAULT_BACKGROUND)
    , m_locale(qgetenv("LANG"))
    , m_name("...")
    , m_desktopBackgrounds(DEFAULT_BACKGROUND)
    , m_limitsInfo(new QMap<int, LimitsInfo>())
{
}

User::User(const User &user)
    : QObject(user.parent())
    , m_isAutomaticLogin(user.m_isAutomaticLogin)
    , m_isLogin(user.m_isLogin)
    , m_isNoPasswordLogin(user.m_isNoPasswordLogin)
    , m_isPasswordValid(user.m_isPasswordValid)
    , m_isUse24HourFormat(user.m_isUse24HourFormat)
    , m_expiredDayLeft(user.m_expiredDayLeft)
    , m_expiredState(user.m_expiredState)
    , m_lastAuthType(0)
    , m_shortDateFormat(user.m_shortDateFormat)
    , m_shortTimeFormat(user.m_shortTimeFormat)
    , m_weekdayFormat(user.m_weekdayFormat)
    , m_accountType(user.m_accountType)
    , m_uid(user.m_uid)
    , m_avatar(user.m_avatar)
    , m_fullName(user.m_fullName)
    , m_greeterBackground(user.m_greeterBackground)
    , m_keyboardLayout(user.m_keyboardLayout)
    , m_locale(user.m_locale)
    , m_name(user.m_name)
    , m_passwordHint(user.m_passwordHint)
    , m_desktopBackgrounds(user.m_desktopBackgrounds)
    , m_keyboardLayoutList(user.m_keyboardLayoutList)
    , m_limitsInfo(new QMap<int, LimitsInfo>(*user.m_limitsInfo))
{
}

User::~User()
{
    delete m_limitsInfo;
}

bool User::operator==(const User &user) const
{
    return user.uid() == m_uid && user.name() == m_name;
}

/**
 * @brief 更新登录状态
 *
 * @param isLogin
 */
void User::updateLoginState(const bool isLogin)
{
    if (isLogin == m_isLogin) {
        return;
    }
    m_isLogin = isLogin;
    emit loginStateChanged(isLogin);
}

/**
 * @brief 更新账户限制信息
 *
 * @param info
 */
void User::updateLimitsInfo(const QString &info)
{
    LimitsInfo limitsInfoTmp;
    const QJsonDocument limitsInfoDoc = QJsonDocument::fromJson(info.toUtf8());
    const QJsonArray limitsInfoArr = limitsInfoDoc.array();
    for (const QJsonValue &limitsInfoStr : limitsInfoArr) {
        const QJsonObject limitsInfoObj = limitsInfoStr.toObject();
        limitsInfoTmp.unlockSecs = limitsInfoObj["unlockSecs"].toVariant().toUInt();
        limitsInfoTmp.maxTries = limitsInfoObj["maxTries"].toVariant().toUInt();
        limitsInfoTmp.numFailures = limitsInfoObj["numFailures"].toVariant().toUInt();
        limitsInfoTmp.locked = limitsInfoObj["locked"].toBool();
        limitsInfoTmp.unlockTime = limitsInfoObj["unlockTime"].toString();
        m_limitsInfo->insert(limitsInfoObj["flag"].toInt(), limitsInfoTmp);
    }
    emit limitsInfoChanged(m_limitsInfo);
}

/**
 * @brief 设置上次成功的认证
 *
 * @param type
 */
void User::setLastAuthType(const int type)
{
    if (m_lastAuthType == type) {
        return;
    }
    m_lastAuthType = type;
}

bool User::checkUserIsNoPWGrp(const User *user) const
{
    if (user->type() == User::ADDomain) {
        return false;
    }

    // Caution: 32 here is unreliable, and there may be more
    // than this number of groups that the user joins.

    int ngroups = 32;
    gid_t groups[32];
    struct passwd *pw = nullptr;
    struct group *gr = nullptr;

    /* Fetch passwd structure (contains first group ID for user) */
    pw = getpwnam(user->name().toUtf8().data());
    if (pw == nullptr) {
        qDebug() << "fetch passwd structure failed, username: " << user->name();
        return false;
    }

    /* Retrieve group list */

    if (getgrouplist(user->name().toUtf8().data(), pw->pw_gid, groups, &ngroups) == -1) {
        fprintf(stderr, "getgrouplist() returned -1; ngroups = %d\n",
                ngroups);
        return false;
    }

    /* Display list of retrieved groups, along with group names */

    for (int i = 0; i < ngroups; i++) {
        gr = getgrgid(groups[i]);
        if (gr != nullptr && QString(gr->gr_name) == QString("nopasswdlogin")) {
            return true;
        }
    }

    return false;
}

QString User::toLocalFile(const QString &path) const
{
    QUrl url(path);

    if (url.isLocalFile()) {
        return url.path();
    } else {
        return url.url();
    }
}

QString User::userPwdName(const uid_t uid) const
{
    //服务器版root用户的uid为0,需要特殊处理
    if (uid < 1000 && uid != 0)
        return QString();

    struct passwd *pw = nullptr;
    /* Fetch passwd structure (contains first group ID for user) */
    pw = getpwuid(uid);

    return pw ? QString().fromLocal8Bit(pw->pw_name) : QString();
}
