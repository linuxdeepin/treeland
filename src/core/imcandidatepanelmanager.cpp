// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "imcandidatepanelmanager.h"
#include "shellhandler.h"
#include "rootsurfacecontainer.h"
#include "surface/surfacewrapper.h"
#include "output/output.h"

#include <WInputMethodHelper>
#include <WXWayland>
#include <WXWaylandSurface>
#include <wxdgtoplevelsurface.h>
#include <WSurface>

#include <qwxwaylandsurface.h>

#include <xcb/xcb.h>

const QLatin1String IMCandidatePanelManager::IM_CANDIDATE_PANEL("org.deepin.treeland.im-candidate-panel");

IMCandidatePanelManager::IMCandidatePanelManager(ShellHandler *shellHandler,
                               WAYLIB_SERVER_NAMESPACE::WInputMethodHelper *inputMethodHelper,
                               QObject *parent)
    : QObject(parent)
    , m_shellHandler(shellHandler)
    , m_inputMethodHelper(inputMethodHelper)
{
    connect(inputMethodHelper, &WAYLIB_SERVER_NAMESPACE::WInputMethodHelper::textInputCursorRectChanged, this,
            &IMCandidatePanelManager::onTextInputCursorRectChanged);

    connect(m_shellHandler, &ShellHandler::surfaceWrapperAboutToRemove, this,
            [this](SurfaceWrapper *wrapper) {
                m_imCandidatePanels.removeAll(wrapper);
            });
}

void IMCandidatePanelManager::setupXWayland(WAYLIB_SERVER_NAMESPACE::WXWayland *xwayland)
{
    m_xwayland = xwayland;
    m_imCandidatePanelAtom = xwayland->atom("_DEEPIN_IM_CANDIDATE_PANEL");
    xwayland->setAtomSupported(m_imCandidatePanelAtom, true);

    connect(xwayland, &WAYLIB_SERVER_NAMESPACE::WXWayland::surfacePropertyChanged, this,
            &IMCandidatePanelManager::onXwaylandPropertyChanged);
}

bool IMCandidatePanelManager::isIMCandidatePanel(SurfaceWrapper *wrapper) const
{
    return wrapper->isIMCandidatePanel();
}

bool IMCandidatePanelManager::isIMCandidatePanel(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface) const
{
    if (m_imCandidatePanelAtom == XCB_ATOM_NONE)
        return false;

    auto conn = m_xwayland->xcbConnection();
    auto prop = Xcb::Property(conn, surface->handle()->handle()->window_id,
                              m_imCandidatePanelAtom, XCB_ATOM_CARDINAL);
    auto data = prop.toByteArray();

    // Guard against truncated or empty property value (CARDINAL requires 4 bytes)
    if (!data || data->size() < static_cast<int>(sizeof(uint32_t)))
        return false;

    // The property value 1 means "this is an IM candidate panel".
    // Use memcpy to avoid strict-aliasing UB; QByteArray::constData() only guarantees
    // char alignment, which is insufficient for uint32_t on some architectures.
    uint32_t value;
    memcpy(&value, data->constData(), sizeof(uint32_t));
    return value == 1;
}

bool IMCandidatePanelManager::checkAndApplyIMCandidatePanel(SurfaceWrapper *wrapper, WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface)
{
    if (surface->tag() != IM_CANDIDATE_PANEL)
        return false;
    if (wrapper->isIMCandidatePanel() || wrapper->container() == m_shellHandler->popupContainer())
        return false;
    applyIMCandidatePanel(wrapper);
    return true;
}

bool IMCandidatePanelManager::checkAndApplyIMCandidatePanel(SurfaceWrapper *wrapper, WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface)
{
    if (!isIMCandidatePanel(surface))
        return false;
    if (wrapper->isIMCandidatePanel() || wrapper->container() == m_shellHandler->popupContainer())
        return false;
    applyIMCandidatePanel(wrapper);
    return true;
}

void IMCandidatePanelManager::applyIMCandidatePanel(SurfaceWrapper *wrapper)
{
    auto *popupContainer = m_shellHandler->popupContainer();

    auto *currentContainer = wrapper->container();
    if (currentContainer)
        currentContainer->removeSurface(wrapper);

    wrapper->setIMCandidatePanel(true);
    popupContainer->addSurface(wrapper);
    wrapper->setHasInitializeContainer(true);
    wrapper->setPositionAutomatic(false);
    wrapper->setXwaylandPositionFromSurface(false);

    if (wrapper->surfaceItem())
        wrapper->surfaceItem()->setFocusPolicy(Qt::NoFocus);

    wrapper->setAcceptKeyboardFocus(false);

    if (!wrapper->noDecoration())
        wrapper->setNoDecoration(true);

    if (!wrapper->skipDockPreView())
        m_shellHandler->foreignToplevel()->removeSurface(wrapper);

    wrapper->disableWindowAnimation();

    m_imCandidatePanels.append(wrapper);

    onTextInputCursorRectChanged(m_inputMethodHelper->textInputCursorRect());
}

void IMCandidatePanelManager::onTextInputCursorRectChanged(QRect cursorRect)
{
    if (cursorRect.isEmpty() || m_imCandidatePanels.isEmpty())
        return;

    auto *focusSurface = m_inputMethodHelper->textInputFocusSurface();
    auto *rootContainer = m_shellHandler->rootSurfaceContainer();
    auto *focusWrapper = focusSurface ? rootContainer->getSurface(focusSurface) : nullptr;
    if (!focusWrapper)
        return;

    auto *output = focusWrapper->ownsOutput();
    if (!output)
        return;

    QPointF pos;
    auto wrapperPos = focusWrapper->mapToGlobal(QPointF(0, 0));
    pos.setX(wrapperPos.x() + cursorRect.x());
    pos.setY(wrapperPos.y() + cursorRect.y() + cursorRect.height() + focusWrapper->titlebarGeometry().height());

    for (auto *wrapper : m_imCandidatePanels) {
        auto normalGeo = wrapper->normalGeometry();
        output->adjustToOutputBounds(pos, normalGeo, output->geometry());
        wrapper->moveNormalGeometryInOutput(pos);
    }
}

void IMCandidatePanelManager::onXwaylandPropertyChanged(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface,
                                                        xcb_atom_t atom)
{
    if (atom != m_imCandidatePanelAtom)
        return;

    auto *rootContainer = m_shellHandler->rootSurfaceContainer();
    auto *wrapper = rootContainer->getSurface(surface);
    if (!wrapper)
        return;

    checkAndApplyIMCandidatePanel(wrapper, surface);
}
