// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QGuiApplication>

#include <QLocalSocket>
#include <QtWaylandCompositor/QWaylandCompositor>

class QQmlApplicationEngine;

namespace TreeLand {
class WaylandSocketProxy;

struct TreeLandAppContext {
    bool isTestMode;
    QString socket;
};

class TreeLand : public QObject {
    Q_OBJECT

public:
    explicit TreeLand(TreeLandAppContext context);

Q_SIGNALS:
    void socketDisconnected();
    void requestAddNewSocket(const QString &username, int fd);

private Q_SLOTS:
    void connected();
    void disconnected();
    void readyRead();
    void error();

private:
    void setup();

private:
    TreeLandAppContext m_context;
    QLocalSocket *m_socket;
    QLocalSocket *m_helperSocket;
    QQmlApplicationEngine *m_engine;
    WaylandSocketProxy *m_socketProxy;
};
}
