// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>

#include "modules/activation/activationmanagerinterfacev1.h"

#include <QObject>

class WaylandServer;
class SurfaceWrapper;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSeat;
class WServer;
WAYLIB_SERVER_END_NAMESPACE

WAYLIB_SERVER_USE_NAMESPACE

class FocusController : public QObject
{
    Q_OBJECT
public:
    explicit FocusController(WaylandServer *server);

    void installActivationProtocol(WServer *server);
    void activateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason,
                         WSeat *seat, bool raise = true);
    void forceActivateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason,
                              WSeat *seat);
    void requestKeyboardFocus(SurfaceWrapper *wrapper, Qt::FocusReason reason,
                              WSeat *seat);
    SurfaceWrapper *keyboardFocusSurface() const;
    SurfaceWrapper *activatedSurface() const;
    void setActivatedSurface(SurfaceWrapper *wrapper, WSeat *seat, bool raise = true);
    WSeat *getLastInteractingSeat(SurfaceWrapper *surface) const;
    void updateSurfaceSeatInteraction(SurfaceWrapper *surface, WSeat *seat);
    void onRenderWindowActiveFocusItemChanged();

private:
    bool canTransferActivation(WSurface *surface, WSeat *seat) const;
    void onActivationRequested(ActivationManagerInterfaceV1::TokenDisposition disposition,
                               WSurface *surface, WSeat *seat);
private:
    WaylandServer *m_server;
};
