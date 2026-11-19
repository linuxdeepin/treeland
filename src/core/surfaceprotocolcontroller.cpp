// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "surfaceprotocolcontroller.h"
#include "session/sessionmanager.h"
#include "waylandserver.h"
#include "showdesktop/showdesktop.h"
#include "rootsurfacecontainer.h"
#include "shellhandler.h"
#include "session/session.h"
#include "session/sessionprotocols.h"
#include "input/inputmanager.h"
#include "modules/output-manager/outputmanagement.h"
#include "modules/wine-window-management/winewindowmanagement.h"
#include "modules/wine-window-state/winewindowstate.h"
#include "wallpaper/wallpapermanager.h"
#include "modules/wallpaper/wallpapermanagerinterfacev1.h"
#include "surface/surfacewrapper.h"
#include "workspace/workspace.h"
#include "modules/personalization/personalizationmanagerinterfacev1.h"
#include "modules/decoration/decorationmanagerinterfacev1.h"
#include "modules/region-watch/regionwatchmanagerinterfacev1.h"
#include "common/treelandlogging.h"
#include "modules/dde-shell/ddeshellattached.h"

#include <wforeigntoplevelv1.h>
#include <wextforeigntoplevellistv1.h>
#include <wxdgdialogmanagerv1.h>
#include <wxdgtopleveltagmanager.h>
#include <wxdgtoplevelsurface.h>
#include <wlayersurface.h>
#ifndef DISABLE_XWAYLAND
#include <wxwaylandsurface.h>
#endif
#include <wserver.h>
#include <wsocket.h>

#include <wlr_all.h>

SurfaceProtocolController::SurfaceProtocolController(WaylandServer *server)
    : QObject(server),
    m_server(server)
{}

void SurfaceProtocolController::installRegionWatch(WServer *server)
{
    m_regionWatchManagerInterfaceV1 = server->attach<TreelandRegionWatchManagerInterfaceV1>();
}

void SurfaceProtocolController::installToplevels(ShellHandler *shell)
{
    connect(shell,
            &ShellHandler::surfaceWrapperAdded,
            this,
            &SurfaceProtocolController::onSurfaceWrapperAdded);

    connect(shell,
            &ShellHandler::surfaceWrapperAboutToRemove,
            this,
            &SurfaceProtocolController::onSurfaceWrapperAboutToRemove);
}

void SurfaceProtocolController::connectForeignToplevel(WForeignToplevel *manager)
{
    connect(manager, &WForeignToplevel::requestActivate, this,
            [this](WToplevelSurface *surface) {
                if (auto *wrapper = m_server->rootContainer()->getSurface(surface))
                    m_server->forceActivateSurface(wrapper, Qt::OtherFocusReason);
            });
    connect(manager, &WForeignToplevel::requestMaximize, this,
            [this](WToplevelSurface *surface, bool maximized) {
                if (auto *wrapper = m_server->rootContainer()->getSurface(surface)) {
                    if (maximized)
                        wrapper->maximize();
                    else
                        wrapper->unmaximize();
                }
            });
    connect(manager, &WForeignToplevel::requestMinimize, this,
            [this](WToplevelSurface *surface, bool minimized) {
                auto *wrapper = m_server->rootContainer()->getSurface(surface);
                if (!wrapper)
                    return;
                if (m_server->showDesktopState() == ShowDesktop::State::Show) {
                    m_server->forceActivateSurface(wrapper);
                } else if (minimized) {
                    wrapper->minimize();
                } else {
                    wrapper->restoreFromMinimized();
                }
            });
    connect(manager, &WForeignToplevel::requestFullscreen, this,
            [this](WToplevelSurface *surface, bool fullscreen) {
                if (auto *wrapper = m_server->rootContainer()->getSurface(surface)) {
                    if (fullscreen)
                        wrapper->enterFullscreen();
                    else
                        wrapper->leaveFullscreen();
                }
            });
    connect(manager, &WForeignToplevel::requestClose, this,
            [this](WToplevelSurface *surface) {
                if (auto *wrapper = m_server->rootContainer()->getSurface(surface))
                    wrapper->close();
            });
}

void SurfaceProtocolController::initializeSession(Session *session)
{
    auto *xdgDecoration = session->protocols()->xdgDecoration();
    connect(xdgDecoration,
            &WXdgDecorationManager::surfaceModeChanged,
            this,
            &SurfaceProtocolController::onSurfaceModeChanged);

    auto *xdgDialog = session->protocols()->xdgDialog();
    connect(xdgDialog,
            &WXdgDialogManagerV1::surfaceModalChanged,
            this,
            [this](WXdgToplevelSurface *toplevel, bool modal) {
                if (auto *wrapper = m_server->rootContainer()->getSurface(toplevel)) {
                    wrapper->setModal(modal);
                } else {
                    qCWarning(lcTlShell) << "xdg-dialog-v1: no wrapper for toplevel" << toplevel;
                }
            });

    auto *foreign = new WForeignToplevel;
    auto *list = new WExtForeignToplevelListV1;
    session->attachInterface(foreign);
    session->attachInterface(list);
    connectForeignToplevel(foreign);
    session->m_foreignToplevel = foreign;
    session->m_toplevelList = list;
    auto *windowManagement = new ForeignToplevelManagerInterfaceV3(session);
    session->m_windowManagement = windowManagement;
    session->attachInterface(windowManagement);
    auto *inputProtocol = new TreelandInputManagerInterfaceV1;
    session->attachInterface(inputProtocol);
    connect(inputProtocol, &TreelandInputManagerInterfaceV1::mouseSettingsCreated,
            session->inputManager(), &InputManager::onMouseSettingsCreated);
    connect(inputProtocol, &TreelandInputManagerInterfaceV1::touchpadSettingsCreated,
            session->inputManager(), &InputManager::onTouchpadSettingsCreated);
    connect(inputProtocol, &TreelandInputManagerInterfaceV1::keyboardSettingsCreated,
            session->inputManager(), &InputManager::onKeyboardSettingsCreated);
    auto *wallpaperProtocol = new TreelandWallpaperManagerInterfaceV1(session);
    session->attachInterface(wallpaperProtocol);
    connect(wallpaperProtocol, &TreelandWallpaperManagerInterfaceV1::wallpaperCreated,
            session->wallpaperManager(), &WallpaperManager::onWallpaperAdded);
    session->attachInterface(new OutputManagerV2(session));
    session->attachInterface(new WineWindowManager);
    session->attachInterface(new WineWindowStateManager);
    m_server->shellHandler()->registerSession(session);
}

void SurfaceProtocolController::onSurfaceModeChanged(WSurface *surface, WXdgDecorationManager::DecorationMode mode)
{
    auto s = m_server->rootContainer()->getSurface(surface);
    if (!s) {
        return;
    }

    s->setNoDecoration(mode != WXdgDecorationManager::Server);
}

void SurfaceProtocolController::addForeignToplevel(SurfaceWrapper *wrapper)
{
    if (auto *session = wrapper->session()) {
        session->foreignToplevel()->addSurface(wrapper->shellSurface());
        session->toplevelList()->addSurface(wrapper->shellSurface());
    }
}

void SurfaceProtocolController::removeForeignToplevel(SurfaceWrapper *wrapper)
{
    if (auto *session = wrapper->session()) {
        session->foreignToplevel()->removeSurface(wrapper->shellSurface());
        session->toplevelList()->removeSurface(wrapper->shellSurface());
    }
}

void SurfaceProtocolController::onSurfaceWrapperAdded(SurfaceWrapper *wrapper)
{
    if (wrapper->isIMCandidatePanel() || !wrapper->session())
        return;
    auto *protocols = wrapper->session()->protocols();
    auto *xdgDecoration = protocols->xdgDecoration();

    bool isXdgToplevel = wrapper->type() == SurfaceWrapper::Type::XdgToplevel;
    bool isXdgPopup = wrapper->type() == SurfaceWrapper::Type::XdgPopup;
    bool isXwayland = wrapper->type() == SurfaceWrapper::Type::XWayland;
    bool isLayer = wrapper->type() == SurfaceWrapper::Type::Layer;

    connect(wrapper->session(),
            &Session::aboutToBeDestroyed,
            wrapper,
            &SurfaceWrapper::closeSurface);

    if (isXdgToplevel || isXdgPopup || isLayer) {
        auto *attached =
            new Personalization(wrapper->shellSurface(), protocols->personalization(), wrapper);
        connect(wrapper, &SurfaceWrapper::aboutToBeInvalidated,
                attached, &Personalization::deleteLater);

        // New SSD personalization protocol (treeland-decoration-unstable-v1).
        // The frozen Personalization above handles the deprecated
        // treeland-personalization-manager-v1 window context; per-window
        // customization for the new protocol is applied via Decoration.
        //
        // An xdg popup negotiates no xdg-decoration and has no titlebar of its
        // own: its frame follows the parent's decoration state. This must run
        // BEFORE the Decoration adapter below — its constructor applies an
        // existing decoration context, and that opt-in must win over the
        // parent-following default.
        if (isXdgPopup) {
            wrapper->setNoTitleBar(true);
            wrapper->setNoDecoration(wrapper->parentSurface()
                                     && wrapper->parentSurface()->noDecoration());
        }

        auto *decoration =
            new Decoration(wrapper->shellSurface(), protocols->decoration(), wrapper);
        connect(wrapper, &SurfaceWrapper::aboutToBeInvalidated,
                decoration, &Decoration::deleteLater);

        // COMPAT: dde-shell still drives the quick launchpad through the
        // deprecated personalization protocol, whose NoTitleBar state used to
        // switch the wrapper back to compositor decoration (SSD frame). Keep
        // that side effect only for the quick launchpad so it keeps drawing
        // its frame during the migration period.
        // TODO: drop once dde-shell migrates the quick launchpad to the new
        // treeland-decoration protocol (together with the deprecated
        // personalization protocol).
        if (wrapper->isQuickLaunchpad() && attached->noTitlebar()) {
            wrapper->setNoDecoration(false);
        }

        auto updateBlur = [attached] {
            attached->surfaceWrapper()->setBlurPersonalization(attached->backgroundType() == Personalization::BackgroundType::Blur);
        };
        connect(attached, &Personalization::backgroundTypeChanged, this, updateBlur);

        // NOTE: corner radius is written by BOTH the deprecated personalization
        // protocol (here) and the new decoration protocol (Decoration::applyContext).
        // The two are not arbitrated; if a client uses both, the last signal to
        // arrive wins. This is intentional: clients are expected to use only one
        // of the two protocols. When the deprecated personalization protocol is
        // removed, drop this block and its setRadius() call — the decoration
        // protocol already drives SurfaceWrapper::setRadius() on its own.
        auto updateCornerRadius = [attached] {
            // The deprecated protocol has no "unset" state, so its 0 keeps meaning
            // "use the compositor default" rather than "square corners".
            const auto radius = attached->cornerRadius();
            attached->surfaceWrapper()->setRadius(radius > 0 ? radius : -1);
        };
        connect(attached, &Personalization::cornerRadiusChanged, this, updateCornerRadius);
        updateCornerRadius();
        updateBlur();
        if (wrapper->isLaunchpad())
            wrapper->setCoverEnabled(true);

        // Titlebar arbitration: only toplevels have an SSD titlebar.
        // Popups and layer shells never draw one, so they never reach here.
        if (isXdgToplevel) {
            auto updateNoTitlebar = [attached, decoration] {
                const bool decorationHides = decoration->hasTitlebarOverride()
                    && decoration->titlebarHidden();

                attached->surfaceWrapper()->setNoTitleBar(attached->noTitlebar()
                                                          || decorationHides);
            };

            connect(attached, &Personalization::windowStateChanged, this, updateNoTitlebar);
            connect(decoration, &Decoration::titlebarOverrideChanged, this, updateNoTitlebar);
            updateNoTitlebar();

            // Replay the mode in case xdg-decoration was negotiated before this
            // wrapper existed; later changes go through onSurfaceModeChanged().
            wrapper->setNoDecoration(xdgDecoration->modeBySurface(wrapper->surface())
                                     != WXdgDecorationManager::Server);
        }
    }

    if (isXwayland) {
#ifndef DISABLE_XWAYLAND
        auto xwaylandSurface = qobject_cast<WXWaylandSurface *>(wrapper->shellSurface());
        auto updateDecorationTitleBar = [wrapper, xwaylandSurface, sessionManager = m_server->sessionManager()]() {
            auto *xwayland = xwaylandSurface->xwayland();
            const auto session = xwayland
                ? sessionManager->sessionForXWayland(xwayland)
                : std::shared_ptr<Session>();
            xcb_atom_t noTitlebarAtom = XCB_ATOM_NONE;
            if (session)
                noTitlebarAtom = session->noTitlebarAtom();
            const auto flags = xwaylandSurface->effectiveDecorationsFlags(noTitlebarAtom);
            wrapper->setNoTitleBar(flags & WXWaylandSurface::DecorationsNoTitle);
            wrapper->setNoDecoration(flags & WXWaylandSurface::DecorationsNoBorder);
        };
        // When x11 surface dissociate, SurfaceWrapper will be destroyed immediately
        // but WXWaylandSurface will not, so must connect to `wrapper`
        QObject::connect(xwaylandSurface, &WXWaylandSurface::bypassManagerChanged,
                                     wrapper,
                                     updateDecorationTitleBar);
        QObject::connect(xwaylandSurface, &WXWaylandSurface::decorationsFlagsChanged,
                                     wrapper,
                                     updateDecorationTitleBar);
        updateDecorationTitleBar();

        wrapper->setHideByWorkspace(!m_server->surfaceBelongsToCurrentSession(wrapper));
#endif
    }

    if (!isLayer) {
        [[maybe_unused]] auto windowOverlapChecker = new WindowOverlapChecker(wrapper, wrapper);

        if (m_regionWatchManagerInterfaceV1)
            m_regionWatchManagerInterfaceV1->addSurface(wrapper);
    }

#ifndef DISABLE_DDM
    if (isLayer) {
        connect(m_server, &WaylandServer::currentModeChanged, wrapper, [this, wrapper] {
            wrapper->setHideByLockScreen(m_server->currentMode() == WaylandServer::CurrentMode::LockScreen);
        });
        wrapper->setHideByLockScreen(m_server->currentMode() == WaylandServer::CurrentMode::LockScreen);
    }
#endif

    if (!wrapper->skipDockPreView()) {
        addForeignToplevel(wrapper);
    }
    connect(wrapper, &SurfaceWrapper::skipDockPreViewChanged, this, [this, wrapper] {
        if (wrapper->skipDockPreView()) {
            removeForeignToplevel(wrapper);
        } else {
            addForeignToplevel(wrapper);
        }
    });
}

void SurfaceProtocolController::onSurfaceWrapperAboutToRemove(SurfaceWrapper *wrapper)
{
    if (wrapper->isIMCandidatePanel())
        return;

    if (m_regionWatchManagerInterfaceV1)
        m_regionWatchManagerInterfaceV1->removeSurface(wrapper);

    if (!wrapper->skipDockPreView()) {
        removeForeignToplevel(wrapper);
    }
    // Ensure the wrapper is removed from active history early to avoid cascading on half-invalid entries
    if (wrapper && wrapper->workspaceId() != -1) {
        auto ws = qobject_cast<Workspace *>(wrapper->container());
        if (ws) {
            Q_ASSERT(ws == wrapper->container());
            ws->removeActivedSurface(wrapper);
        }
    }
}
