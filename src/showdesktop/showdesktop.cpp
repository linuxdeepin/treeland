// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "showdesktop.h"
#include "core/waylandserver.h"
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "core/layersurfacecontainer.h"
#include "output/outputmanager.h"
#include "seat/seatmanager.h"
#include "surface/surfacewrapper.h"

#include <wlayersurface.h>
#include <wserver.h>

ShowDesktop::ShowDesktop(WaylandServer *server, OutputManager *outputManager)
    : QObject(server),
    m_server(server),
    m_outputManager(outputManager)
{}

void ShowDesktop::initialize(WServer *server)
{
    m_showDesktopInterfaceV1 = server->attach<ShowDesktopInterfaceV1>();
}

void ShowDesktop::connectProtocol()
{
    connect(m_showDesktopInterfaceV1, &ShowDesktopInterfaceV1::desktopStateChanged,
            this, &ShowDesktop::onShowDesktop);
}

ShowDesktopInterfaceV1::State ShowDesktop::state() const
{
    return m_showDesktop;
}

void ShowDesktop::setState(ShowDesktopInterfaceV1::State state)
{
    m_showDesktopInterfaceV1->setDesktopState(state);
}

void ShowDesktop::onShowDesktop()
{
    ShowDesktopInterfaceV1::State s = m_showDesktopInterfaceV1->desktopState();
    if (m_showDesktop == s
        || (s != ShowDesktopInterfaceV1::State::Normal
            && s != ShowDesktopInterfaceV1::State::Show))
        return;

    m_showDesktop = s;
    Q_EMIT stateChanged();
    const auto &surfaces = m_outputManager->workspaceSurfaces();
    for (auto &surface : surfaces) {
        if (surface->isMinimized()) {
            continue;
        }
        if (s == ShowDesktopInterfaceV1::State::Normal) {
            surface->startShowDesktopAnimation(true);
        } else if (s == ShowDesktopInterfaceV1::State::Show) {
            surface->startShowDesktopAnimation(false);
        }
    }

    if (s == ShowDesktopInterfaceV1::State::Show) {
        // Find the desktop background surface first
        SurfaceWrapper *desktopSurface = nullptr;
        const auto &backgroundSurfaces = m_server->shellHandler()->backgroundContainer()->surfaces();
        for (SurfaceWrapper *w : backgroundSurfaces) {
            auto *layer = qobject_cast<WLayerSurface *>(w->shellSurface());
            if (layer && layer->scope() == QStringLiteral("dde-shell/desktop")) {
                desktopSurface = w;
                break;
            }
        }

        const auto &seats = m_server->seatManager()->seats();
        for (auto *seat : seats) {
            auto *seatContainer = m_server->rootContainer()->getSeatContainer(seat);
            // Seat may be in transition (hotplug); requestKeyboardFocus asserts
            // on a missing seat container.
            if (!seatContainer)
                continue;

            // Dismiss any popup keyboard grab: the grab would redirect keyboard
            // focus back to the popup and defeat the desktop-surface transfer.
            seatContainer->dismissPopups();
            // Move keyboard focus to the desktop surface (or drop it when the
            // desktop surface is unavailable) so layer-shell windows that close
            // on focus loss exit.
            m_server->requestKeyboardFocus(desktopSurface, Qt::OtherFocusReason, seat);
        }
    } else if (s == ShowDesktopInterfaceV1::State::Normal) {
        // m_showDesktop already set to s above; the protocol state is already Normal.
        restoreShowDesktopFocus();
    }
}

void ShowDesktop::restoreShowDesktopFocus()
{
    const auto &seats = m_server->seatManager()->seats();
    for (auto *seat : seats) {
        if (auto *seatContainer = m_server->rootContainer()->getSeatContainer(seat))
            seatContainer->restoreShowDesktopFocus();
    }
}

void ShowDesktop::cancelShowDesktop(SurfaceWrapper *excludeSurface)
{
    if (m_showDesktop != ShowDesktopInterfaceV1::State::Show)
        return;
    m_showDesktop = ShowDesktopInterfaceV1::State::Normal;
    m_showDesktopInterfaceV1->setDesktopState(ShowDesktopInterfaceV1::State::Normal);
    const auto &surfaces = m_outputManager->workspaceSurfaces();
    for (auto &surface : surfaces) {
        if (surface == excludeSurface)
            continue;
        if (!surface->isMinimized() && !surface->isVisible()) {
            surface->setHideByShowDesk(true);
            surface->minimize(/*onAnimation=*/ false);
        }
    }
}

void ShowDesktop::restoreFromShowDesktop(SurfaceWrapper *activeSurface)
{
    if (m_showDesktop != ShowDesktopInterfaceV1::State::Show)
        return;
    cancelShowDesktop(activeSurface);
    if (activeSurface) {
        activeSurface->restoreFromMinimized();
    }
    restoreShowDesktopFocus();
}

bool ShowDesktop::isAvailable() const
{
    return m_showDesktopInterfaceV1 != nullptr;
}
