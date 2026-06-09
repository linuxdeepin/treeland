// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "dummytreelandclient.h"

#include <rep_treelandremote_replica.h>
#include <QElapsedTimer>

#include <QRemoteObjectNode>

DummyTreelandClient::DummyTreelandClient(QObject *parent)
    : QObject(parent)
{
}

DummyTreelandClient::~DummyTreelandClient() = default;

void DummyTreelandClient::setTreelandUrl(const QUrl &url)
{
    if (m_url == url)
        return;

    m_url = url;
    m_replica.reset();
    m_node.reset();
}

bool DummyTreelandClient::ensureConnected()
{
    if (m_replica)
        return true;
    if (!m_url.isValid() || m_url.isEmpty())
        return false;

    auto node = std::make_unique<QRemoteObjectNode>();
    if (!node->connectToNode(m_url))
        return false;

    auto replica = std::unique_ptr<DDMTreelandRemoteReplica>(node->acquire<DDMTreelandRemoteReplica>());
    connect(replica.get(), &DDMTreelandRemoteReplica::lockChanged, this, &DummyTreelandClient::lockChanged);
    m_node = std::move(node);
    m_replica = std::move(replica);
    return true;
}

bool DummyTreelandClient::waitForReady(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        if (ensureConnected() && m_replica->waitForSource(200))
            return true;

        m_replica.reset();
        m_node.reset();
    }

    return false;
}

bool DummyTreelandClient::lock()
{
    if (!waitForReady(3000))
        return false;
    m_replica->lock();
    return true;
}

bool DummyTreelandClient::switchToUser(const QString &user)
{
    if (!waitForReady(3000))
        return false;
    m_replica->switchToUser(user);
    return true;
}

bool DummyTreelandClient::lockState() const
{
    if (!m_replica)
        return false;
    auto reply = m_replica->lockState();
    reply.waitForFinished();
    return reply.error() == QRemoteObjectPendingCall::NoError && reply.returnValue();
}
