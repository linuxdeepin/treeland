// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "waylandsocketproxy.h"
#include "quick/protocols/qwpersonalizationmanager.h"

#include <QGuiApplication>
#include <QLocalSocket>
#include <QtWaylandCompositor/QWaylandCompositor>

class QQmlApplicationEngine;

namespace TreeLand {

struct TreeLandAppContext {
    QString socket;
};

class TreeLand : public QObject {
    Q_OBJECT
    Q_PROPERTY(WaylandSocketProxy* socketProxy READ socketProxy WRITE setSocketProxy)
    Q_PROPERTY(QuickPersonalizationManager* personalManager READ personalManager WRITE setPersonalManager)
    Q_PROPERTY(bool testMode READ testMode CONSTANT)

public:
    explicit TreeLand(TreeLandAppContext context);

    inline WaylandSocketProxy* socketProxy() const {
        return m_socketProxy;
    }

    inline QuickPersonalizationManager* personalManager() const {
        return m_personalManager;
    }

    Q_INVOKABLE void retranslate() noexcept;

    bool testMode() const;

Q_SIGNALS:
    void socketDisconnected();

private Q_SLOTS:
    void connected();
    void disconnected();
    void readyRead();
    void error();

private:
    void setup();
    void setSocketProxy(WaylandSocketProxy *socketProxy);
    void setPersonalManager(QuickPersonalizationManager *manager);

private:
    TreeLandAppContext m_context;
    QLocalSocket *m_socket;
    QLocalSocket *m_helperSocket;
    QQmlApplicationEngine *m_engine;
    WaylandSocketProxy *m_socketProxy;
    QuickPersonalizationManager *m_personalManager;
};
}
