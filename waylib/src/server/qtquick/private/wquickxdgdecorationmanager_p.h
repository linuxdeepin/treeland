// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>

#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WQuickXdgDecorationManagerPrivate;
class WAYLIB_SERVER_EXPORT WQuickXdgDecorationManager : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickXdgDecorationManager)
    Q_PROPERTY(DecorationMode mode READ mode WRITE setMode NOTIFY modeChanged)
    QML_NAMED_ELEMENT(XdgDecorationManager)

public:
    explicit WQuickXdgDecorationManager(QObject *parent = nullptr);

    enum DecorationMode {
        DecidesByClient,
        PreferClientSide,
        PreferServerSide,
    };
    Q_ENUM(DecorationMode)

    void setMode(DecorationMode mode);
    DecorationMode mode() const;

Q_SIGNALS:
    void modeChanged(DecorationMode mode);

private:
    void create() override;
};

WAYLIB_SERVER_END_NAMESPACE
