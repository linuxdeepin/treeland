// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>

#include <wglobal.h>

#include <xcb/xcb.h>

class SurfaceWrapper;
class ShellHandler;
class RootSurfaceContainer;

namespace WAYLIB_SERVER_NAMESPACE {
class WInputMethodHelper;
class WXdgToplevelSurface;
class WXWayland;
class WXWaylandSurface;
}

class IMCandidatePanelManager : public QObject
{
    Q_OBJECT

public:
    explicit IMCandidatePanelManager(ShellHandler *shellHandler,
                             WAYLIB_SERVER_NAMESPACE::WInputMethodHelper *inputMethodHelper,
                             QObject *parent = nullptr);

    void setupXWayland(WAYLIB_SERVER_NAMESPACE::WXWayland *xwayland);

    bool isIMCandidatePanel(SurfaceWrapper *wrapper) const;
    bool isIMCandidatePanel(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface) const;
    bool checkAndApplyIMCandidatePanel(SurfaceWrapper *wrapper, WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    bool checkAndApplyIMCandidatePanel(SurfaceWrapper *wrapper, WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface);

private:
    void applyIMCandidatePanel(SurfaceWrapper *wrapper);

    static const QLatin1String IM_CANDIDATE_PANEL;
    void onTextInputCursorRectChanged(QRect cursorRect);
    void onXwaylandPropertyChanged(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface,
                                  xcb_atom_t atom);

    ShellHandler *m_shellHandler;
    WAYLIB_SERVER_NAMESPACE::WInputMethodHelper *m_inputMethodHelper;
    WAYLIB_SERVER_NAMESPACE::WXWayland *m_xwayland = nullptr;
    xcb_atom_t m_imCandidatePanelAtom = XCB_ATOM_NONE;
    QList<SurfaceWrapper*> m_imCandidatePanels;
};
