// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QUrl>

#include <memory>

class QRemoteObjectNode;
class DDMTreelandRemoteReplica;

class DummyTreelandClient : public QObject
{
    Q_OBJECT
public:
    explicit DummyTreelandClient(QObject *parent = nullptr);
    ~DummyTreelandClient() override;

    void setTreelandUrl(const QUrl &url);
    bool ensureConnected();
    bool waitForReady(int timeoutMs);
    bool lock();
    bool switchToUser(const QString &user);
    bool lockState() const;

Q_SIGNALS:
    void lockChanged(bool locked);

private:
    QUrl m_url;
    std::unique_ptr<QRemoteObjectNode> m_node;
    std::unique_ptr<DDMTreelandRemoteReplica> m_replica;
};
