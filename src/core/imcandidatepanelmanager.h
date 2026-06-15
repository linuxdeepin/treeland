// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <xcb/xcb.h>

#include <wglobal.h>

#include <QObject>
#include <QPointer>

class SurfaceWrapper;
class ShellHandler;
class RootSurfaceContainer;
class Output;

namespace WAYLIB_SERVER_NAMESPACE {
class WInputMethodHelper;
class WXdgToplevelSurface;
class WXWayland;
class WXWaylandSurface;
} // namespace WAYLIB_SERVER_NAMESPACE

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
    bool checkAndApplyIMCandidatePanel(SurfaceWrapper *wrapper,
                                       WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    bool checkAndApplyIMCandidatePanel(SurfaceWrapper *wrapper,
                                       WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface);
    xcb_atom_t imCandidatePanelAtom() const;

    // Parse IM candidate panel property from async result map
    static bool parseIMCandidatePanelProperty(const QMap<xcb_atom_t, QByteArray> &result,
                                              xcb_atom_t atom);

private:
    void applyIMCandidatePanel(SurfaceWrapper *wrapper);
    void arrangeIMCandidatePanels(Output *output, const QPointF &basePos);
    void onIMCandidatePanelGeometryChanged();

    static const QLatin1String IM_CANDIDATE_PANEL;
    void applyInitialPositionAtCursor();
    void onXwaylandPropertyChanged(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface,
                                   xcb_atom_t atom);

    ShellHandler *m_shellHandler;
    WAYLIB_SERVER_NAMESPACE::WInputMethodHelper *m_inputMethodHelper;
    QPointer<WAYLIB_SERVER_NAMESPACE::WXWayland> m_xwayland;
    xcb_atom_t m_imCandidatePanelAtom = XCB_ATOM_NONE;
    QList<SurfaceWrapper *> m_imCandidatePanels;
};
