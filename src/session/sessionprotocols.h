// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "sessioncomponent.h"
#include <QPointer>
#include <wglobal.h>

class PersonalizationManagerInterfaceV1;
class DecorationManagerInterfaceV1;
class WindowTransitionManagerInterfaceV1;

WAYLIB_SERVER_BEGIN_NAMESPACE
class WXdgShell;
class WLayerShell;
class WXdgDecorationManager;
class WXdgDialogManagerV1;
class WXdgToplevelTagManagerV1;
WAYLIB_SERVER_END_NAMESPACE

// A fixed protocol domain within the shared wl_display. Protocol instances
// remain owned by this Session even when another Session becomes active.
class SessionProtocols : public SessionComponent
{
    Q_OBJECT
    QML_ANONYMOUS
public:
    explicit SessionProtocols(Session *session);
    WAYLIB_SERVER_NAMESPACE::WXdgShell *xdgShell() const;
    WAYLIB_SERVER_NAMESPACE::WLayerShell *layerShell() const;
    WAYLIB_SERVER_NAMESPACE::WXdgDecorationManager *xdgDecoration() const;
    WAYLIB_SERVER_NAMESPACE::WXdgDialogManagerV1 *xdgDialog() const;
    WAYLIB_SERVER_NAMESPACE::WXdgToplevelTagManagerV1 *xdgTags() const;
    PersonalizationManagerInterfaceV1 *personalization() const;
    DecorationManagerInterfaceV1 *decoration() const;
    WindowTransitionManagerInterfaceV1 *windowTransitions() const;

private:
    QPointer<WAYLIB_SERVER_NAMESPACE::WXdgShell> m_xdgShell;
    QPointer<WAYLIB_SERVER_NAMESPACE::WLayerShell> m_layerShell;
    QPointer<WAYLIB_SERVER_NAMESPACE::WXdgDecorationManager> m_xdgDecoration;
    QPointer<WAYLIB_SERVER_NAMESPACE::WXdgDialogManagerV1> m_xdgDialog;
    QPointer<WAYLIB_SERVER_NAMESPACE::WXdgToplevelTagManagerV1> m_xdgTags;
    QPointer<WindowTransitionManagerInterfaceV1> m_windowTransitions;
    QPointer<PersonalizationManagerInterfaceV1> m_personalization;
    QPointer<DecorationManagerInterfaceV1> m_decoration;
};
