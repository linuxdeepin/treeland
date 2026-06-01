// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "imcandidatepanelmanager.h"

#include "output/output.h"
#include "rootsurfacecontainer.h"
#include "shellhandler.h"
#include "surface/surfacewrapper.h"

#include <xcb/xcb.h>

#include <WInputMethodHelper>
#include <WSurface>
#include <WXWayland>
#include <WXWaylandSurface>
#include <wxdgtoplevelsurface.h>

#include <qwxwaylandsurface.h>

const QLatin1String IMCandidatePanelManager::IM_CANDIDATE_PANEL(
    "org.deepin.treeland.im-candidate-panel");

IMCandidatePanelManager::IMCandidatePanelManager(
    ShellHandler *shellHandler,
    WAYLIB_SERVER_NAMESPACE::WInputMethodHelper *inputMethodHelper,
    QObject *parent)
    : QObject(parent)
    , m_shellHandler(shellHandler)
    , m_inputMethodHelper(inputMethodHelper)
{
    connect(m_shellHandler,
            &ShellHandler::surfaceWrapperAboutToRemove,
            this,
            [this](SurfaceWrapper *wrapper) {
                m_imCandidatePanels.removeAll(wrapper);
            });
}

void IMCandidatePanelManager::setupXWayland(WAYLIB_SERVER_NAMESPACE::WXWayland *xwayland)
{
    m_xwayland = xwayland;
    m_imCandidatePanelAtom = xwayland->atom("_DEEPIN_IM_CANDIDATE_PANEL");
    xwayland->setAtomSupported(m_imCandidatePanelAtom, true);

    connect(xwayland,
            &WAYLIB_SERVER_NAMESPACE::WXWayland::windowPropertyChanged,
            this,
            &IMCandidatePanelManager::onXwaylandPropertyChanged);
}

bool IMCandidatePanelManager::isIMCandidatePanel(SurfaceWrapper *wrapper) const
{
    return wrapper->isIMCandidatePanel();
}

bool IMCandidatePanelManager::isIMCandidatePanel(
    WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface) const
{
    if (m_imCandidatePanelAtom == XCB_ATOM_NONE || !m_xwayland)
        return false;

    auto conn = m_xwayland->xcbConnection();
    if (!conn)
        return false;

    auto prop = Xcb::Property(conn,
                              surface->handle()->handle()->window_id,
                              m_imCandidatePanelAtom,
                              XCB_ATOM_CARDINAL);
    auto data = prop.toByteArray();

    // Guard against truncated or empty property value (CARDINAL requires 4 bytes)
    if (data.size() < static_cast<int>(sizeof(uint32_t)))
        return false;

    // The property value 1 means "this is an IM candidate panel".
    // Use memcpy to avoid strict-aliasing UB; QByteArray::constData() only guarantees
    // char alignment, which is insufficient for uint32_t on some architectures.
    uint32_t value;
    memcpy(&value, data.constData(), sizeof(uint32_t));
    return value == 1;
}

bool IMCandidatePanelManager::checkAndApplyIMCandidatePanel(
    SurfaceWrapper *wrapper,
    WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface)
{
    if (surface->tag() != IM_CANDIDATE_PANEL)
        return false;
    if (wrapper->isIMCandidatePanel())
        return false;
    applyIMCandidatePanel(wrapper);
    return true;
}

bool IMCandidatePanelManager::checkAndApplyIMCandidatePanel(
    SurfaceWrapper *wrapper,
    WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface)
{
    if (!isIMCandidatePanel(surface))
        return false;
    if (wrapper->isIMCandidatePanel())
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

    connect(wrapper,
            &SurfaceWrapper::normalGeometryChanged,
            this,
            &IMCandidatePanelManager::onIMCandidatePanelGeometryChanged);

    applyInitialPositionAtCursor();
}

void IMCandidatePanelManager::applyInitialPositionAtCursor()
{
    auto cursorRect = m_inputMethodHelper->textInputCursorRect();
    if (cursorRect.isEmpty() || m_imCandidatePanels.isEmpty())
        return;

    auto *focusSurface = m_inputMethodHelper->textInputFocusSurface();
    if (!focusSurface)
        return;

    auto *rootContainer = m_shellHandler->rootSurfaceContainer();
    auto *focusWrapper = rootContainer->getSurface(focusSurface);
    if (!focusWrapper)
        return;

    auto *output = focusWrapper->ownsOutput();
    if (!output)
        return;

    QPointF pos;
    auto wrapperPos = focusWrapper->mapToGlobal(QPointF(0, 0));
    pos.setX(wrapperPos.x() + cursorRect.x());
    pos.setY(wrapperPos.y() + cursorRect.y() + cursorRect.height()
             + focusWrapper->titlebarGeometry().height());

    arrangeIMCandidatePanels(output, pos);
}

void IMCandidatePanelManager::arrangeIMCandidatePanels(Output *output, const QPointF &basePos)
{
    for (auto *wrapper : m_imCandidatePanels) {
        auto normalGeo = wrapper->normalGeometry();
        // Wait for geometry to be valid before arranging
        if (normalGeo.isEmpty())
            continue;

        auto pos = basePos;
        output->adjustToOutputBounds(pos, normalGeo, output->geometry());
        wrapper->moveNormalGeometryInOutput(pos);
    }
}

void IMCandidatePanelManager::onIMCandidatePanelGeometryChanged()
{
    if (m_imCandidatePanels.isEmpty())
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
    auto cursorRect = m_inputMethodHelper->textInputCursorRect();
    pos.setX(wrapperPos.x() + cursorRect.x());
    pos.setY(wrapperPos.y() + cursorRect.y() + cursorRect.height()
             + focusWrapper->titlebarGeometry().height());

    arrangeIMCandidatePanels(output, pos);
}

void IMCandidatePanelManager::onXwaylandPropertyChanged(
    WAYLIB_SERVER_NAMESPACE::WXWaylandSurface *surface,
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
