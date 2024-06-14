// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <QQmlParserStatus>
#include <QQmlEngine>

Q_MOC_INCLUDE(<wsocket.h>)

WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickWaylandServer;
class WQuickWaylandServerInterfacePrivate;
class WAYLIB_SERVER_EXPORT WQuickWaylandServerInterface : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(WQuickWaylandServerInterface)
    QML_NAMED_ELEMENT(WaylandServerInterface)
    Q_PROPERTY(bool polished READ isPolished NOTIFY afterPolish)
    Q_PROPERTY(WSocket* targetSocket READ targetSocket WRITE setTargetSocket NOTIFY targetSocketChanged)
    Q_PRIVATE_PROPERTY(WQuickWaylandServerInterface::d_func(), QQmlListProperty<WClient> targetClients READ targetClients NOTIFY targetClientsChanged DESIGNABLE false)
    Q_PROPERTY(bool exclusionTargetClients READ exclusionTargetClients WRITE setExclusionTargetClients NOTIFY exclusionTargetClientsChanged FINAL)

public:
    explicit WQuickWaylandServerInterface(QObject *parent = nullptr);

    WQuickWaylandServer *server() const;
    bool isPolished() const;

    WSocket *targetSocket() const;
    void setTargetSocket(WSocket *socket);

    bool exclusionTargetClients() const;
    void setExclusionTargetClients(bool newExclusionTargetClients);

Q_SIGNALS:
    void beforeCreate();
    void afterPolish();
    void targetSocketChanged();
    void targetClientsChanged();
    void exclusionTargetClientsChanged();

protected:
    friend class WQuickWaylandServer;
    friend class WQuickWaylandServerPrivate;

    [[nodiscard]] virtual WServerInterface *create();
    virtual void polish();

    void doCreate();
    void doPolish();
};

class WQuickWaylandServerPrivate;
class WAYLIB_SERVER_EXPORT WQuickWaylandServer : public WServer, public QQmlParserStatus
{
    Q_OBJECT
    QML_NAMED_ELEMENT(WaylandServer)
    W_DECLARE_PRIVATE(WQuickWaylandServer)
    Q_PRIVATE_PROPERTY(WQuickWaylandServer::d_func(), QQmlListProperty<WQuickWaylandServerInterface> interfaces READ interfaces NOTIFY interfacesChanged DESIGNABLE false)
    Q_PRIVATE_PROPERTY(WQuickWaylandServer::d_func(), QQmlListProperty<QObject> children READ children DESIGNABLE false)
    Q_CLASSINFO("DefaultProperty", "interfaces")
    Q_INTERFACES(QQmlParserStatus)

public:
    explicit WQuickWaylandServer(QObject *parent = nullptr);

Q_SIGNALS:
    void interfacesChanged();
    void started();

private:
    void classBegin() override;
    void componentComplete() override;
};

WAYLIB_SERVER_END_NAMESPACE
