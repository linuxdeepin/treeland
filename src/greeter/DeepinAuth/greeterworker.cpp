// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DeepinAuth/greeterworker.h"

#include "DeepinAuth/deepinauthframework.h"
#include "DeepinAuth/authcommon.h"
#include "DeepinAuth/sessionbasemodel.h"

#include <pwd.h>

#define LOCKSERVICE_PATH "/org/deepin/dde/LockService1"
#define LOCKSERVICE_NAME "org.deepin.dde.LockService1"
#define SECURITYENHANCE_PATH "/com/deepin/daemon/SecurityEnhance"
#define SECURITYENHANCE_NAME "com.deepin.daemon.SecurityEnhance"

using namespace AuthCommon;

const int NUM_LOCKED = 0;   // 禁用小键盘
const int NUM_UNLOCKED = 1; // 启用小键盘
const int NUM_LOCK_UNKNOWN = 2; // 未知状态

GreeterWorker::GreeterWorker(QObject *parent)
    : QObject(parent)
    , m_authFramework(new DeepinAuthFramework(this))
    , m_proxy(new SDDM::GreeterProxy(this))
    , m_model(new SessionBaseModel(this))
{
    registerMFAInfoListMetaType();

    initConnections();
    initData();
    initConfiguration();
}

GreeterWorker::~GreeterWorker()
{
}

void GreeterWorker::initConnections()
{
    /* org.deepin.dde.Authenticate1 */
    connect(m_authFramework, &DeepinAuthFramework::FramworkStateChanged, m_model, &SessionBaseModel::updateFrameworkState);
    connect(m_authFramework, &DeepinAuthFramework::LimitsInfoChanged, this, [this](const QString &account) {
        qDebug() << "GreeterWorker::initConnections LimitsInfoChanged:" << account;
        if (account == m_model->currentUser()->name()) {
            m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(account));
        }
    });
    connect(m_authFramework, &DeepinAuthFramework::SupportedEncryptsChanged, m_model, &SessionBaseModel::updateSupportedEncryptionType);
    connect(m_authFramework, &DeepinAuthFramework::SupportedMixAuthFlagsChanged, m_model, &SessionBaseModel::updateSupportedMixAuthFlags);
    /* org.deepin.dde.Authenticate1.Session */
    connect(m_authFramework, &DeepinAuthFramework::AuthStateChanged, this, &GreeterWorker::onAuthStateChanged);
    connect(m_authFramework, &DeepinAuthFramework::FactorsInfoChanged, m_model, &SessionBaseModel::updateFactorsInfo);
    connect(m_authFramework, &DeepinAuthFramework::FuzzyMFAChanged, m_model, &SessionBaseModel::updateFuzzyMFA);
    connect(m_authFramework, &DeepinAuthFramework::MFAFlagChanged, m_model, &SessionBaseModel::updateMFAFlag);
    connect(m_authFramework, &DeepinAuthFramework::PINLenChanged, m_model, &SessionBaseModel::updatePINLen);
    connect(m_authFramework, &DeepinAuthFramework::PromptChanged, m_model, &SessionBaseModel::updatePrompt);
    /* model */
    connect(m_model, &SessionBaseModel::authTypeChanged, this, [ = ](const int type) {
        qInfo() << "Auth type changed: " << type;
        if (type > 0 && m_model->getAuthProperty().MFAFlag) {
            startAuthentication(m_account, m_model->getAuthProperty().AuthType);
        }
    });
    // 等保服务开启/关闭时更新
    QDBusConnection::systemBus().connect(SECURITYENHANCE_NAME, SECURITYENHANCE_PATH, SECURITYENHANCE_NAME,
                                         "Receipt", this, SLOT(onReceiptChanged(bool)));
}

void GreeterWorker::initData()
{
    /* org.deepin.dde.Authenticate1 */
    /* org.deepin.dde.Authenticate1 */
    if (m_authFramework->isDeepinAuthValid()) {
        m_model->updateFrameworkState(m_authFramework->GetFrameworkState());
        m_model->updateSupportedEncryptionType(m_authFramework->GetSupportedEncrypts());
        m_model->updateSupportedMixAuthFlags(m_authFramework->GetSupportedMixAuthFlags());
        // m_model->updateLimitedInfo(m_authFramework->GetLimitedInfo(m_model->currentUser()->name()));
    }}

void GreeterWorker::initConfiguration()
{
}

SDDM::SessionModel *GreeterWorker::sessionModel() const
{
    return m_proxy->sessionModel();
}

void GreeterWorker::setSessionModel(SDDM::SessionModel *model)
{
    m_proxy->setSessionModel(model);
}

SDDM::UserModel *GreeterWorker::userModel() const
{
    return m_proxy->userModel();
}

void GreeterWorker::setUserModel(SDDM::UserModel *model)
{
    m_proxy->setUserModel(model);
}

bool GreeterWorker::visible() const
{
    return m_proxy->visible();
}

bool GreeterWorker::isSecurityEnhanceOpen()
{
    QDBusInterface securityEnhanceInterface(SECURITYENHANCE_NAME,
                               SECURITYENHANCE_PATH,
                               SECURITYENHANCE_NAME,
                               QDBusConnection::systemBus());
    QDBusReply<QString> reply = securityEnhanceInterface.call("Status");
    if (!reply.isValid()) {
       qWarning() << "get security enhance status error: " << reply.error();
       return false;
    }
    return reply.value() == "open" || reply.value() == "opening";
}

/**
 * @brief 创建认证服务
 * 有用户时，通过dbus发过来的user信息创建认证服务，类服务器模式下通过用户输入的用户创建认证服务
 * @param account
 */
void GreeterWorker::createAuthentication(const QString &account)
{
    qInfo() << "GreeterWorker::createAuthentication:" << account;

    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->CreateAuthController(account, m_authFramework->GetSupportedMixAuthFlags(), Login);
        m_authFramework->SetAuthQuitFlag(account, DeepinAuthFramework::ManualQuit);
        // if (!m_authFramework->SetPrivilegesEnable(account, QString("/usr/sbin/lightdm"))) {
        //     qWarning() << "Failed to set privileges!";
        // }
        startGreeterAuth(account);
        break;
    default:
        startGreeterAuth(account);
        // m_model->setAuthType(AT_PAM);
        break;
    }
}

/**
 * @brief 退出认证服务
 *
 * @param account
 */
void GreeterWorker::destroyAuthentication(const QString &account)
{
    qDebug() << "GreeterWorker::destroyAuthentication:" << account;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->DestroyAuthController(account);
        break;
    default:
        break;
    }
}

/**
 * @brief 开启认证服务    -- 作为接口提供给上层，屏蔽底层细节
 *
 * @param account   账户
 * @param authType  认证类型（可传入一种或多种）
 * @param timeout   设定超时时间（默认 -1）
 */
void GreeterWorker::startAuthentication(const QString &account, const int authType)
{
    qDebug() << "GreeterWorker::startAuthentication:" << account << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        m_authFramework->StartAuthentication(account, authType, -1);
        // 如果密码被锁定了，lightdm会停止验证，在开启验证的时候需要判断一下lightdm是否在验证中。
        // if (!m_greeter->inAuthentication())
            // startGreeterAuth(account);

        startGreeterAuth(account);
        break;
    default:
        startGreeterAuth(account);
        break;
    }
}

/**
 * @brief 将密文发送给认证服务
 *
 * @param account   账户
 * @param authType  认证类型
 * @param token     密文
 */
void GreeterWorker::sendTokenToAuth(const QString &account, const int authType, const QString &token)
{
    qDebug() << "GreeterWorker::sendTokenToAuth:" << account << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        if (AT_PAM != authType) {
            m_authFramework->SendTokenToAuth(account, authType, token);
        }
        break;
    default:
        // respond(token);
        break;
    }
}

/**
 * @brief 结束本次认证，下次认证前需要先开启认证服务
 *
 * @param account   账户
 * @param authType  认证类型
 */
void GreeterWorker::endAuthentication(const QString &account, const int authType)
{
    qDebug() << "GreeterWorker::endAuthentication:" << account << authType;
    switch (m_model->getAuthProperty().FrameworkState) {
    case Available:
        if (authType == AT_All)
            m_authFramework->SetPrivilegesDisable(account);

        m_authFramework->EndAuthentication(account, authType);
        break;
    default:
        break;
    }
}

/**
 * @brief 认证完成
 */
void GreeterWorker::authenticationComplete()
{
    // endAuthentication(m_account, AT_All);
    // destroyAuthentication(m_account);
}

void GreeterWorker::onAuthFinished()
{
    qInfo() << "GreeterWorker::onAuthFinished";
}

void GreeterWorker::onAuthStateChanged(const int type, const int state, const QString &message)
{
    qDebug() << "GreeterWorker::onAuthStateChanged:" << type << state << message;
    if (m_model->getAuthProperty().MFAFlag) {
        if (type == AT_All) {
            switch (state) {
            case AS_Success:
                break;
            case AS_Cancel:
                // destroyAuthentication(m_account);
                break;
            default:
                break;
            }
        } else {
            switch (state) {
            case AS_Success:
                // m_model->updateAuthState(type, state, message);
                break;
            case AS_Failure:
                // endAuthentication(m_account, type);
                // 人脸和虹膜需要手动重启验证
                // if (!m_model->currentUser()->limitsInfo(type).locked && type != AT_Face && type != AT_Iris) {
                //     QTimer::singleShot(50, this, [this, type] {
                //         startAuthentication(m_account, type);
                //     });
                // }
                // QTimer::singleShot(50, this, [ = ] {
                //     m_model->updateAuthState(type, state, message);
                // });
                break;
            case AS_Locked:
                // endAuthentication(m_account, type);
                // TODO: 信号时序问题,考虑优化,Bug 89056
                QTimer::singleShot(50, this, [ = ] {
                    // m_model->updateAuthState(type, state, message);
                });
                break;
            case AS_Timeout:
            case AS_Error:
                // endAuthentication(m_account, type);
                break;
            case AS_Unlocked:
                break;
            default:
                break;
            }
        }
    } else {
        switch (state) {
        case AS_Success:
            if (type == AT_Face || type == AT_Iris)
                // m_resetSessionTimer->start();
            break;
        case AS_Failure:
            if (AT_All != type) {
                // endAuthentication(m_account, type);
                // 人脸和虹膜需要手动重新开启验证
                // if (!m_model->currentUser()->limitsInfo(type).locked && type != AT_Face && type != AT_Iris) {
                //     QTimer::singleShot(50, this, [this, type] {
                //         startAuthentication(m_account, type);
                //     });
                // }
            }
            break;
        case AS_Cancel:
            // destroyAuthentication(m_account);
            break;
        default:
            break;
        }
    }
}

void GreeterWorker::onReceiptChanged(bool state)
{
    Q_UNUSED(state)
}

void GreeterWorker::startGreeterAuth(const QString &account)
{
    m_proxy->login(account, "", 0);
}

void GreeterWorker::changePasswd()
{
}
