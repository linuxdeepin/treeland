// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "dummyddmservice.h"

#include "dummytreelandclient.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRemoteObjectHost>
#include <QTextStream>
#include <QUrl>

namespace {
constexpr auto dummyRefreshMessage = "__dummyddm_refresh_state__";

SessionEntry toRemoteSessionEntry(const DummySessionEntry &entry)
{
    SessionEntry remote;
    remote.setFileName(entry.fileName);
    remote.setType(entry.type);
    remote.setDisplayName(entry.displayName);
    remote.setComment(entry.comment);
    remote.setExec(entry.exec);
    return remote;
}

void printLine(const QString &message)
{
    QTextStream(stderr) << message << Qt::endl;
}

class DummyDdmControlSourceImpl : public DummyDdmControlSource
{
public:
    explicit DummyDdmControlSourceImpl(DummyDdmService *service)
        : DummyDdmControlSource(service)
        , m_service(service)
    {
    }

    bool lock() override
    {
        return m_service->ensureTreelandConnected() && m_service->treelandClient()->lock();
    }

    bool unlock() override
    {
        const QString user = !m_service->state().currentUser.isEmpty() ? m_service->state().currentUser
                                                                       : m_service->state().lastUser;
        if (!m_service->ensureTreelandConnected())
            return false;
        if (!m_service->treelandClient()->switchToUser(user))
            return false;

        int sessionId = 1;
        bool inserted = true;
        for (const auto &entry : std::as_const(m_service->state().activeSessions)) {
            if (entry.user == user) {
                sessionId = entry.sessionId;
                inserted = false;
                break;
            }
            sessionId = qMax(sessionId, entry.sessionId + 1);
        }
        if (inserted)
            m_service->state().activeSessions.append({ user, sessionId });

        Q_EMIT m_service->userSessionAdded(user, sessionId);
        return true;
    }

    bool switchUser(QString user) override
    {
        m_service->state().currentUser = user;
        return m_service->ensureTreelandConnected() && m_service->treelandClient()->switchToUser(user);
    }

    bool setCurrentUser(QString user) override
    {
        m_service->state().currentUser = user;
        return true;
    }

    bool setLastUser(QString user) override
    {
        m_service->state().lastUser = user;
        Q_EMIT m_service->informationMessage(QString::fromLatin1(dummyRefreshMessage));
        return true;
    }

    bool setLastSession(QString sessionFile) override
    {
        m_service->state().lastSession = sessionFile;
        Q_EMIT m_service->informationMessage(QString::fromLatin1(dummyRefreshMessage));
        return true;
    }

    bool setRememberLastSession(bool enabled) override
    {
        m_service->state().rememberLastSession = enabled;
        Q_EMIT m_service->informationMessage(QString::fromLatin1(dummyRefreshMessage));
        return true;
    }

    bool setSessionsJson(QString stateFile) override
    {
        DummyDdmState loaded;
        if (!loaded.loadFromFile(stateFile))
            return false;
        m_service->state().sessions = loaded.sessions;
        Q_EMIT m_service->informationMessage(QString::fromLatin1(dummyRefreshMessage));
        return true;
    }

    bool addUserSession(QString user, int id) override
    {
        m_service->state().activeSessions.append({ user, id });
        Q_EMIT m_service->userSessionAdded(user, id);
        return true;
    }

    bool removeUserSession(QString user, int id) override
    {
        auto &sessions = m_service->state().activeSessions;
        for (auto it = sessions.begin(); it != sessions.end(); ++it) {
            if (it->user == user && it->sessionId == id) {
                sessions.erase(it);
                Q_EMIT m_service->userSessionRemoved(user, id);
                return true;
            }
        }
        return false;
    }

    bool setCapabilities(bool powerOff,
                         bool reboot,
                         bool suspend,
                         bool hibernate,
                         bool hybridSleep) override
    {
        m_service->state().canPowerOff = powerOff;
        m_service->state().canReboot = reboot;
        m_service->state().canSuspend = suspend;
        m_service->state().canHibernate = hibernate;
        m_service->state().canHybridSleep = hybridSleep;
        Q_EMIT m_service->informationMessage(QString::fromLatin1(dummyRefreshMessage));
        return true;
    }

    bool emitLoginFailed(QString user) override
    {
        Q_EMIT m_service->loginFailed(user);
        return true;
    }

    bool emitInfo(QString message) override
    {
        Q_EMIT m_service->informationMessage(message);
        return true;
    }

    QString statusJson() override
    {
        QJsonObject root;
        root.insert(QStringLiteral("hostName"), m_service->state().hostName);
        root.insert(QStringLiteral("currentUser"), m_service->state().currentUser);
        root.insert(QStringLiteral("lastUser"), m_service->state().lastUser);
        root.insert(QStringLiteral("lastSession"), m_service->state().lastSession);
        root.insert(QStringLiteral("rememberLastSession"), m_service->state().rememberLastSession);
        root.insert(QStringLiteral("treelandLocked"), m_service->state().treelandLocked);

        QJsonObject capabilities;
        capabilities.insert(QStringLiteral("powerOff"), m_service->state().canPowerOff);
        capabilities.insert(QStringLiteral("reboot"), m_service->state().canReboot);
        capabilities.insert(QStringLiteral("suspend"), m_service->state().canSuspend);
        capabilities.insert(QStringLiteral("hibernate"), m_service->state().canHibernate);
        capabilities.insert(QStringLiteral("hybridSleep"), m_service->state().canHybridSleep);
        root.insert(QStringLiteral("capabilities"), capabilities);

        QJsonArray sessions;
        for (const auto &entry : std::as_const(m_service->state().sessions)) {
            QJsonObject object;
            object.insert(QStringLiteral("fileName"), entry.fileName);
            object.insert(QStringLiteral("type"), entry.type);
            object.insert(QStringLiteral("displayName"), entry.displayName);
            object.insert(QStringLiteral("comment"), entry.comment);
            object.insert(QStringLiteral("exec"), entry.exec);
            sessions.append(object);
        }
        root.insert(QStringLiteral("sessions"), sessions);

        QJsonArray activeSessions;
        for (const auto &entry : std::as_const(m_service->state().activeSessions)) {
            QJsonObject object;
            object.insert(QStringLiteral("user"), entry.user);
            object.insert(QStringLiteral("sessionId"), entry.sessionId);
            activeSessions.append(object);
        }
        root.insert(QStringLiteral("activeSessions"), activeSessions);
        return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }

private:
    DummyDdmService *m_service = nullptr;
};
}

DummyDdmService::DummyDdmService(QObject *parent)
    : DDMRemoteSource(parent)
{
}

DummyDdmService::~DummyDdmService() = default;

bool DummyDdmService::start(const QUrl &ddmUrl, const QString &stateFile, const QUrl &treelandUrl)
{
    if (!m_state.loadFromFile(stateFile)) {
        qCritical() << "failed to load dummy ddm state from" << stateFile;
        return false;
    }

    m_treelandUrl = treelandUrl;
    m_host = std::make_unique<QRemoteObjectHost>(ddmUrl, this);
    setHostName(m_state.hostName);
    if (!m_host->enableRemoting(this, QStringLiteral("DDMRemote"))) {
        qCritical() << "failed to enable DDMRemote on" << ddmUrl;
        return false;
    }

    m_control = new DummyDdmControlSourceImpl(this);
    if (!m_host->enableRemoting(m_control, QStringLiteral("DummyDdmControl"))) {
        qCritical() << "failed to enable DummyDdmControl on" << ddmUrl;
        return false;
    }

    m_treeland = std::make_unique<DummyTreelandClient>(this);
    m_treeland->setTreelandUrl(treelandUrl);
    connect(m_treeland.get(), &DummyTreelandClient::lockChanged, this, [this](bool locked) {
        m_state.treelandLocked = locked;
    });

    qInfo() << "DDMRemote ready on" << ddmUrl;
    printLine(QStringLiteral("DDMRemote ready on %1").arg(ddmUrl.toString()));
    return true;
}

QString DummyDdmService::hostName() const
{
    return m_state.hostName;
}

void DummyDdmService::setHostName(QString hostName)
{
    m_state.hostName = std::move(hostName);
}

DummyDdmState &DummyDdmService::state()
{
    return m_state;
}

DummyTreelandClient *DummyDdmService::treelandClient() const
{
    return m_treeland.get();
}

bool DummyDdmService::ensureTreelandConnected(int timeoutMs)
{
    if (!m_treeland)
        return false;
    return m_treeland->waitForReady(timeoutMs);
}

bool DummyDdmService::canPowerOff() { return m_state.canPowerOff; }
bool DummyDdmService::canReboot() { return m_state.canReboot; }
bool DummyDdmService::canSuspend() { return m_state.canSuspend; }
bool DummyDdmService::canHibernate() { return m_state.canHibernate; }
bool DummyDdmService::canHybridSleep() { return m_state.canHybridSleep; }

bool DummyDdmService::connectGreeter()
{
    qInfo() << "connectGreeter";
    printLine(QStringLiteral("connectGreeter"));
    if (!ensureTreelandConnected())
        return false;

    for (const auto &entry : std::as_const(m_state.activeSessions))
        Q_EMIT userSessionAdded(entry.user, entry.sessionId);
    return true;
}

bool DummyDdmService::login(QString user, QString password, int sessionType, QString sessionFile)
{
    Q_UNUSED(password)
    qInfo() << "login" << user << sessionType << sessionFile;
    printLine(QStringLiteral("login %1 %2 %3").arg(user, QString::number(sessionType), sessionFile));
    m_state.currentUser = user;
    m_state.lastUser = user;
    m_state.lastSession = sessionFile;

    int sessionId = 1;
    for (const auto &entry : std::as_const(m_state.activeSessions))
        sessionId = qMax(sessionId, entry.sessionId + 1);
    m_state.activeSessions.append({ user, sessionId });
    Q_EMIT userSessionAdded(user, sessionId);
    return true;
}

bool DummyDdmService::logout(int id)
{
    qInfo() << "logout" << id;
    for (auto it = m_state.activeSessions.begin(); it != m_state.activeSessions.end(); ++it) {
        if (it->sessionId == id) {
            const QString user = it->user;
            m_state.activeSessions.erase(it);
            Q_EMIT userSessionRemoved(user, id);
            return true;
        }
    }
    return false;
}

bool DummyDdmService::powerOff()
{
    qInfo() << "powerOff";
    return true;
}

bool DummyDdmService::reboot()
{
    qInfo() << "reboot";
    return true;
}

bool DummyDdmService::suspend()
{
    qInfo() << "suspend";
    return true;
}

bool DummyDdmService::hibernate()
{
    qInfo() << "hibernate";
    return true;
}

bool DummyDdmService::hybridSleep()
{
    qInfo() << "hybridSleep";
    return true;
}

QList<SessionEntry> DummyDdmService::sessions()
{
    printLine(QStringLiteral("sessions"));
    QList<SessionEntry> remoteSessions;
    remoteSessions.reserve(m_state.sessions.size());
    for (const auto &entry : std::as_const(m_state.sessions))
        remoteSessions.append(toRemoteSessionEntry(entry));
    return remoteSessions;
}

QString DummyDdmService::lastSession()
{
    return m_state.lastSession;
}

QString DummyDdmService::lastUser()
{
    return m_state.lastUser;
}

bool DummyDdmService::rememberLastSession()
{
    return m_state.rememberLastSession;
}
