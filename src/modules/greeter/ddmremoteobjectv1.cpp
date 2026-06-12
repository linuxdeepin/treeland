// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "ddmremoteobjectv1.h"

#include "common/treelandlogging.h"
#include "greeterproxy.h"
#include "rep_treelandremote_source.h"
#include "seat/helper.h"
#include "usermodel.h"

#include <QRemoteObjectHost>
#include <QUrl>

namespace {
constexpr auto remoteObjectName = "DDMTreelandRemote";
constexpr auto remoteObjectUrl = "local:org.deepin.dde.treeland.qro";
} // namespace

class DDMRemoteObjectV1Private : public DDMTreelandRemoteSource
{
public:
    explicit DDMRemoteObjectV1Private(DDMRemoteObjectV1 *qObject);
    bool startHost();
    void setGreeterProxy(GreeterProxy *greeterProxy);
    void stopHost();
    bool isListening() const;
    void switchToUser(QString username) override;
    void lock() override;
    bool lockState() override;

    DDMRemoteObjectV1 *q = nullptr;
    QMetaObject::Connection lockChangedConnection;
    QRemoteObjectHost *host = nullptr;
    bool remotingEnabled = false;
};

DDMRemoteObjectV1Private::DDMRemoteObjectV1Private(DDMRemoteObjectV1 *qObject)
    : DDMTreelandRemoteSource()
    , q(qObject)
{
}

void DDMRemoteObjectV1Private::setGreeterProxy(GreeterProxy *greeterProxy)
{
    if (lockChangedConnection) {
        QObject::disconnect(lockChangedConnection);
        lockChangedConnection = { };
    }

    if (!greeterProxy)
        return;

    lockChangedConnection = QObject::connect(greeterProxy,
                                             &GreeterProxy::lockChanged,
                                             this,
                                             &DDMTreelandRemoteSource::lockChanged);
}

bool DDMRemoteObjectV1Private::startHost()
{
    if (isListening())
        return true;

    host = new QRemoteObjectHost(QUrl(QString::fromLatin1(remoteObjectUrl)), q);
    if (!host->enableRemoting(this, QString::fromLatin1(remoteObjectName))) {
        qCCritical(treelandCore) << "Failed to enable DDM remote object";
        delete host;
        host = nullptr;
        return false;
    }

    remotingEnabled = true;
    qCInfo(treelandCore) << "DDM remote object is listening on" << remoteObjectUrl;
    return true;
}

void DDMRemoteObjectV1Private::stopHost()
{
    remotingEnabled = false;

    if (!host)
        return;

    delete host;
    host = nullptr;
    qCInfo(treelandCore) << "DDM remote object stopped";
}

bool DDMRemoteObjectV1Private::isListening() const
{
    return remotingEnabled && host;
}

void DDMRemoteObjectV1Private::switchToUser(QString username)
{
    qCInfo(treelandCore) << "DDM remote: switchToUser" << username;
    auto helper = Helper::instance();
    if (username != helper->userModel()->currentUserName())
        helper->userModel()->setCurrentUserName(username);
    helper->showLockScreen();
}

bool DDMRemoteObjectV1Private::lockState()
{
    auto *greeterProxy = Helper::instance()->greeterProxy();
    return greeterProxy && greeterProxy->isLocked();
}

void DDMRemoteObjectV1Private::lock()
{
    qCInfo(treelandCore) << "DDM remote: lock";
    Helper::instance()->showLockScreen();
}

DDMRemoteObjectV1::DDMRemoteObjectV1(QObject *parent)
    : QObject(parent)
    , d(new DDMRemoteObjectV1Private(this))
{
}

DDMRemoteObjectV1::~DDMRemoteObjectV1() = default;

bool DDMRemoteObjectV1::start()
{
    return d->startHost();
}

void DDMRemoteObjectV1::setGreeterProxy(GreeterProxy *greeterProxy)
{
    d->setGreeterProxy(greeterProxy);
}

void DDMRemoteObjectV1::stop()
{
    d->stopHost();
}

bool DDMRemoteObjectV1::isListening() const
{
    return d->isListening();
}
