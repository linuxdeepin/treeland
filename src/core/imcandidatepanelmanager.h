// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#ifndef DISABLE_XWAYLAND
#include <xcb/xcb.h>
#endif

#include <wglobal.h>

#include <QObject>
#include <QPointer>

class SurfaceWrapper;
class ShellHandler;
class RootSurfaceContainer;
class Output;

namespace WAYLIB_SERVER_NAMESPACE {
class WInputMethodHelper;
#ifndef DISABLE_XWAYLAND
class WXWayland;
class WXWaylandSurface;
#endif
} // namespace WAYLIB_SERVER_NAMESPACE

class IMCandidatePanelManager : public QObject
{
    Q_OBJECT

public:
    explicit IMCandidatePanelManager(ShellHandler *shellHandler,
                                     WAYLIB_SERVER_NAMESPACE::WInputMethodHelper *inputMethodHelper,
                                     QObject *parent = nullptr);

    void setInputMethodHelper(WAYLIB_SERVER_NAMESPACE::WInputMethodHelper *helper);
#ifndef DISABLE_XWAYLAND
    void setupXWayland(WAYLIB_SERVER_NAMESPACE::WXWayland *xwayland);
#endif

    bool isIMCandidatePanel(SurfaceWrapper *wrapper) const;
#ifndef DISABLE_XWAYLAND
    bool isIMCandidatePanel(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface) const;
#endif
    bool checkAndApplyIMCandidatePanel(SurfaceWrapper *wrapper);
#ifndef DISABLE_XWAYLAND
    xcb_atom_t imCandidatePanelAtom() const;

    // Parse IM candidate panel property from async result map
    static bool parseIMCandidatePanelProperty(const QMap<xcb_atom_t, QByteArray> &result,
                                              xcb_atom_t atom);
#endif

private:
    void applyIMCandidatePanel(SurfaceWrapper *wrapper);
    void arrangeIMCandidatePanels(Output *output, const QPointF &basePos);
    void onIMCandidatePanelGeometryChanged();
    void onTextInputFocusSurfaceChanged();

    static const QLatin1String IM_CANDIDATE_PANEL;
    void applyInitialPositionAtCursor();
#ifndef DISABLE_XWAYLAND
    void onXwaylandPropertyChanged(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface,
                                   xcb_atom_t atom);
#endif

    ShellHandler *m_shellHandler;
    QPointer<WAYLIB_SERVER_NAMESPACE::WInputMethodHelper> m_inputMethodHelper;
    QMetaObject::Connection m_inputMethodFocusConnection;
#ifndef DISABLE_XWAYLAND
    QPointer<WAYLIB_SERVER_NAMESPACE::WXWayland> m_xwayland;
    xcb_atom_t m_imCandidatePanelAtom = XCB_ATOM_NONE;
#endif
    QList<SurfaceWrapper *> m_imCandidatePanels;
};
