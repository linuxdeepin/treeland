// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wquickwaylandserver.h>

#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurface;
class WQuickXdgDecorationManager;
class WQuickXdgDecorationManagerPrivate;
class WAYLIB_SERVER_EXPORT WQuickXdgDecorationManagerAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool serverDecorationEnabled READ serverDecorationEnabled NOTIFY serverDecorationEnabledChanged FINAL)
    QML_ANONYMOUS

public:
    WQuickXdgDecorationManagerAttached(WSurface *target, WQuickXdgDecorationManager *manager);

    bool serverDecorationEnabled() const;

Q_SIGNALS:
    void serverDecorationEnabledChanged();

private:
    WSurface *m_target;
    WQuickXdgDecorationManager *m_manager;
};
class WAYLIB_SERVER_EXPORT WQuickXdgDecorationManager : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WQuickXdgDecorationManager)
    Q_PROPERTY(DecorationMode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(WSocket* ownsSocket READ ownsSocket CONSTANT)
    QML_ATTACHED(WQuickXdgDecorationManagerAttached)
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
    void setModeBySurface(WSurface *surface, DecorationMode mode);

    DecorationMode mode() const;
    DecorationMode modeBySurface(WSurface* surface) const;

    static WQuickXdgDecorationManagerAttached *qmlAttachedProperties(QObject *target);

Q_SIGNALS:
    void modeChanged(DecorationMode mode);
    void surfaceModeChanged(WSurface *surface, DecorationMode mode);

private:
    void create() override;
};

WAYLIB_SERVER_END_NAMESPACE
