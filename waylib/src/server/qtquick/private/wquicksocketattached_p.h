// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include <QObject>
#include <QtQmlIntegration>

QT_BEGIN_NAMESPACE
class QLocalServer;
QT_END_NAMESPACE

Q_DECLARE_OPAQUE_POINTER(wl_client*)

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSocket;

class WAYLIB_SERVER_EXPORT WQuickSocketAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WSocket* socket READ socket CONSTANT)
    Q_PROPERTY(WSocket* rootSocket READ rootSocket CONSTANT)
    QML_NAMED_ELEMENT(WaylandSocket)
    QML_UNCREATABLE("Only use for the attached properties.")
    QML_ATTACHED(WQuickSocketAttached)

public:
    explicit WQuickSocketAttached(WSocket *socket);
    static WQuickSocketAttached *qmlAttachedProperties(QObject *target);

    WSocket *socket() const;
    WSocket *rootSocket() const;
};

WAYLIB_SERVER_END_NAMESPACE
