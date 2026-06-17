// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "dummyddmstate.h"

#include <QObject>
#include <QUrl>
#include <rep_ddmremote_source.h>
#include <rep_dummyddmcontrol_source.h>

#include <memory>

class QRemoteObjectHost;
class DummyTreelandClient;

class DummyDdmService : public DDMRemoteSource
{
    Q_OBJECT
public:
    explicit DummyDdmService(QObject *parent = nullptr);
    ~DummyDdmService() override;

    QString hostName() const override;
    void setHostName(QString hostName) override;
    bool start(const QUrl &ddmUrl, const QString &stateFile, const QUrl &treelandUrl);
    DummyDdmState &state();
    DummyTreelandClient *treelandClient() const;
    bool ensureTreelandConnected(int timeoutMs = 10000);

    bool canPowerOff() override;
    bool canReboot() override;
    bool canSuspend() override;
    bool canHibernate() override;
    bool canHybridSleep() override;
    bool connectGreeter() override;
    bool login(QString user, QString password, int sessionType, QString sessionFile) override;
    bool logout(int id) override;
    bool powerOff() override;
    bool reboot() override;
    bool suspend() override;
    bool hibernate() override;
    bool hybridSleep() override;
    QList<SessionEntry> sessions() override;
    QString lastSession() override;
    QString lastUser() override;
    bool rememberLastSession() override;

private:
    DummyDdmState m_state;
    std::unique_ptr<QRemoteObjectHost> m_host;
    std::unique_ptr<DummyTreelandClient> m_treeland;
    QObject *m_control = nullptr;
    QUrl m_treelandUrl;
};
