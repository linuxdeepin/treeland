// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "qwpersonalizationmanager.h"

#include <QDBusContext>
#include <QDBusUnixFileDescriptor>
#include <QGuiApplication>
#include <QLocalSocket>
#include <QtWaylandCompositor/QWaylandCompositor>

#include <wsocket.h>

#include <memory>

class QQmlApplicationEngine;

namespace TreeLand {

struct TreeLandAppContext
{
    QString socket;
    QString run;
};

class TreeLand : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_PROPERTY(QuickPersonalizationManager* personalManager READ personalManager WRITE setPersonalManager)
    Q_PROPERTY(bool testMode READ testMode CONSTANT)

public:
    explicit TreeLand(TreeLandAppContext context);

    inline QuickPersonalizationManager *personalManager() const { return m_personalManager; }

    Q_INVOKABLE void retranslate() noexcept;

    bool testMode() const;

Q_SIGNALS:
    void socketDisconnected();

public Q_SLOTS:
    bool Activate(QDBusUnixFileDescriptor fd);

private Q_SLOTS:
    void connected();
    void disconnected();
    void readyRead();
    void error();

private:
    void setup();
    void setPersonalManager(QuickPersonalizationManager *manager);

private:
    TreeLandAppContext m_context;
    QLocalSocket *m_socket;
    QLocalSocket *m_helperSocket;
    QQmlApplicationEngine *m_engine;
    QuickPersonalizationManager *m_personalManager;
    QMap<QString, std::shared_ptr<Waylib::Server::WSocket>> m_userWaylandSocket;
    QMap<QString, std::shared_ptr<QDBusUnixFileDescriptor>> m_userDisplayFds;
};
} // namespace TreeLand
