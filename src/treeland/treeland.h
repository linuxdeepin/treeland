// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "waylandsocketproxy.h"

#include <QGuiApplication>
#include <QLocalSocket>
#include <QtWaylandCompositor/QWaylandCompositor>

class QQmlApplicationEngine;

namespace TreeLand {

struct TreeLandAppContext {
    bool isTestMode;
    QString socket;
};

class TreeLand : public QObject {
    Q_OBJECT
    Q_PROPERTY(WaylandSocketProxy* socketProxy READ socketProxy WRITE setSocketProxy)
    Q_PROPERTY(bool testMode READ testMode CONSTANT)

public:
    explicit TreeLand(TreeLandAppContext context);

    inline WaylandSocketProxy* socketProxy() const {
        return m_socketProxy;
    }

    inline bool testMode() const {
        return m_context.isTestMode;
    }

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

private:
    TreeLandAppContext m_context;
    QLocalSocket *m_socket;
    QLocalSocket *m_helperSocket;
    QQmlApplicationEngine *m_engine;
    WaylandSocketProxy *m_socketProxy;
};
}
