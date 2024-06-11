// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wquickwaylandserver.h>

QT_BEGIN_NAMESPACE
class QLocalServer;
QT_END_NAMESPACE

Q_DECLARE_OPAQUE_POINTER(wl_client*)

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WQuickSocketAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WSocket* socket READ socket CONSTANT)
    Q_PROPERTY(WSocket* rootSocket READ rootSocket CONSTANT)
    QML_ANONYMOUS

public:
    explicit WQuickSocketAttached(WSocket *socket);

    WSocket *socket() const;
    WSocket *rootSocket() const;
};

class WAYLIB_SERVER_EXPORT WQuickSocket : public WQuickWaylandServerInterface
{
    Q_OBJECT
    Q_PROPERTY(WSocket* socket READ socket NOTIFY socketChanged FINAL)
    Q_PROPERTY(QString socketFile READ socketFile WRITE setSocketFile NOTIFY socketFileChanged FINAL)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(bool freezeClientWhenDisable READ freezeClientWhenDisable WRITE setFreezeClientWhenDisable NOTIFY freezeClientWhenDisableChanged FINAL)
    Q_PROPERTY(WSocket* targetSocket READ targetSocket CONSTANT)
    QML_NAMED_ELEMENT(WaylandSocket)
    QML_ATTACHED(WQuickSocketAttached)

public:
    explicit WQuickSocket(QObject *parent = nullptr);

    static WQuickSocketAttached *qmlAttachedProperties(QObject *target);

    WSocket *socket() const;

    Q_INVOKABLE void addClient(wl_client *client);
    Q_INVOKABLE void removeClient(wl_client *client);
    Q_INVOKABLE void removeClient(WClient *client);

    QString socketFile() const;
    void setSocketFile(const QString &newSocketFile);

    bool enabled() const;
    void setEnabled(bool newEnabled);

    bool freezeClientWhenDisable() const;
    void setFreezeClientWhenDisable(bool newFreezeClientWhenDisable);

Q_SIGNALS:
    void socketChanged();
    void socketFileChanged();
    void enabledChanged();
    void freezeClientWhenDisableChanged();

private:
    void polish() override;
    void setSocket(WSocket *socket);
    void createSocket(const QString &file);

    bool m_enabled = true;
    bool m_freezeClientWhenDisable;
    QString m_socketFile;
    WSocket *m_socket = nullptr;
};

WAYLIB_SERVER_END_NAMESPACE
