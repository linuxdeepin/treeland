// SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GREETERWORKEK_H
#define GREETERWORKEK_H

#include <QObject>
#include <QQmlEngine>

#include "DBusTypes.h"
#include "GreeterProxy.h"

#include "SessionModel.h"
#include "UserModel.h"

class DeepinAuthFramework;
class SessionBaseModel;

class GreeterWorker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(SDDM::SessionModel* sessionModel READ sessionModel WRITE setSessionModel NOTIFY sessionModelChanged)
    Q_PROPERTY(SDDM::UserModel* userModel READ userModel WRITE setUserModel NOTIFY userModelChanged)
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    QML_ELEMENT

public:
    enum AuthFlag {
        Password = 1 << 0,
        Fingerprint = 1 << 1,
        Face = 1 << 2,
        ActiveDirectory = 1 << 3
    };

    explicit GreeterWorker(QObject *parent = nullptr);
    ~GreeterWorker() override;

    SDDM::SessionModel *sessionModel() const;
    void setSessionModel(SDDM::SessionModel *model);

    SDDM::UserModel *userModel() const;
    void setUserModel(SDDM::UserModel *model);

    bool visible() const;

    bool isSecurityEnhanceOpen();

signals:
    void requestShowPrompt(const QString &prompt);
    void requestShowMessage(const QString &message);

    void sessionModelChanged(SDDM::SessionModel *model);
    void userModelChanged(SDDM::UserModel *model);
    void visibleChanged(bool visible);

public slots:
    /* New authentication framework */
    Q_INVOKABLE void createAuthentication(const QString &account);
    Q_INVOKABLE void destroyAuthentication(const QString &account);
    Q_INVOKABLE void startAuthentication(const QString &account, const int authType);
    Q_INVOKABLE void endAuthentication(const QString &account, const int authType);
    Q_INVOKABLE void sendTokenToAuth(const QString &account, const int authType, const QString &token);

    void onAuthFinished();

private slots:
    void onAuthStateChanged(const int type, const int state, const QString &message);
    void onReceiptChanged(bool state);

private:
    void initConnections();
    void initData();
    void initConfiguration();

    void checkDBusServer(bool isValid);
    // void showPrompt(const QString &text, const QLightDM::Greeter::PromptType type);
    // void showMessage(const QString &text, const QLightDM::Greeter::MessageType type);
    void authenticationComplete();
    void startGreeterAuth(const QString &account = QString());
    void changePasswd();

private:
    DeepinAuthFramework *m_authFramework;
    SDDM::GreeterProxy *m_proxy;
    SessionBaseModel*  m_model;
    QString m_account;
};

#endif  // GREETERWORKEK_H
