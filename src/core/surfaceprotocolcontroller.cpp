// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "surfaceprotocolcontroller.h"
#include "waylandserver.h"
#include "rootsurfacecontainer.h"
#include "shellhandler.h"
#include "session/session.h"
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
#include <wxwaylandsurface.h>
#include <wserver.h>

#include <wlr_all.h>

SurfaceProtocolController::SurfaceProtocolController(WaylandServer *server)
    : QObject(server),
    m_server(server)
{}

void SurfaceProtocolController::installRegionWatch(WServer *server)
{
    m_regionWatchManagerInterfaceV1 = server->attach<TreelandRegionWatchManagerInterfaceV1>();
}

void SurfaceProtocolController::installToplevels(WServer *server, ShellHandler *shell)
{
    m_foreignToplevel = server->attach<WForeignToplevel>();
    connect(m_foreignToplevel, &WForeignToplevel::requestActivate, this,
            [this](WToplevelSurface *surface) {
                if (auto *wrapper = m_server->rootContainer()->getSurface(surface))
                    m_server->forceActivateSurface(wrapper, Qt::OtherFocusReason);
            });
    connect(m_foreignToplevel, &WForeignToplevel::requestMaximize, this,
            [this](WToplevelSurface *surface, bool maximized) {
                if (auto *wrapper = m_server->rootContainer()->getSurface(surface)) {
                    if (maximized)
                        wrapper->maximize();
                    else
                        wrapper->unmaximize();
                }
            });
    connect(m_foreignToplevel, &WForeignToplevel::requestMinimize, this,
            [this](WToplevelSurface *surface, bool minimized) {
                auto *wrapper = m_server->rootContainer()->getSurface(surface);
                if (!wrapper)
                    return;
                if (m_server->showDesktopState() == ShowDesktopInterfaceV1::State::Show) {
                    m_server->forceActivateSurface(wrapper);
                } else if (minimized) {
                    wrapper->minimize();
                } else {
                    wrapper->restoreFromMinimized();
                }
            });
    connect(m_foreignToplevel, &WForeignToplevel::requestFullscreen, this,
            [this](WToplevelSurface *surface, bool fullscreen) {
                if (auto *wrapper = m_server->rootContainer()->getSurface(surface)) {
                    if (fullscreen)
                        wrapper->enterFullscreen();
                    else
                        wrapper->leaveFullscreen();
                }
            });
    connect(m_foreignToplevel, &WForeignToplevel::requestClose, this,
            [this](WToplevelSurface *surface) {
                if (auto *wrapper = m_server->rootContainer()->getSurface(surface))
                    wrapper->close();
            });
    m_extForeignToplevelListV1 = server->attach<WExtForeignToplevelListV1>();
    connect(shell,
            &ShellHandler::surfaceWrapperAdded,
            this,
            &SurfaceProtocolController::onSurfaceWrapperAdded);

    connect(shell,
            &ShellHandler::surfaceWrapperAboutToRemove,
            this,
            &SurfaceProtocolController::onSurfaceWrapperAboutToRemove);
}

void SurfaceProtocolController::installPersonalization(WServer *server)
{
    m_personalizationInterfaceV1 = server->attach<PersonalizationManagerInterfaceV1>();
}

void SurfaceProtocolController::installDecorationCustomization(WServer *server)
{
    m_decorationInterfaceV1 = server->attach<DecorationManagerInterfaceV1>();
}

void SurfaceProtocolController::installDecoration(WServer *server)
{
    m_xdgDecorationManager = server->attach<WXdgDecorationManager>();
    connect(m_xdgDecorationManager,
            &WXdgDecorationManager::surfaceModeChanged,
            this,
            &SurfaceProtocolController::onSurfaceModeChanged);

    m_xdgDialogManagerV1 = server->attach<WXdgDialogManagerV1>();
    connect(m_xdgDialogManagerV1,
            &WXdgDialogManagerV1::surfaceModalChanged,
            this,
            [this](WXdgToplevelSurface *toplevel, bool modal) {
                if (auto *wrapper = m_server->rootContainer()->getSurface(toplevel)) {
                    wrapper->setModal(modal);
                } else {
                    qCWarning(lcTlShell) << "xdg-dialog-v1: no wrapper for toplevel" << toplevel;
                }
            });

    m_xdgToplevelTagManagerV1 = server->attach<WXdgToplevelTagManagerV1>();

}

void SurfaceProtocolController::setUserId(uid_t uid)
{
    m_personalizationInterfaceV1->setUserId(uid);
}

WExtForeignToplevelListV1 *SurfaceProtocolController::toplevelList() const
{
    return m_extForeignToplevelListV1;
}

void SurfaceProtocolController::onSurfaceModeChanged(WSurface *surface, WXdgDecorationManager::DecorationMode mode)
{
    auto s = m_server->rootContainer()->getSurface(surface);
    if (!s) {
        return;
    }

    s->setNoDecoration(mode != WXdgDecorationManager::Server);
}

void SurfaceProtocolController::onSurfaceWrapperAdded(SurfaceWrapper *wrapper)
{
    if (wrapper->isIMCandidatePanel())
        return;

    bool isXdgToplevel = wrapper->type() == SurfaceWrapper::Type::XdgToplevel;
    bool isXdgPopup = wrapper->type() == SurfaceWrapper::Type::XdgPopup;
    bool isXwayland = wrapper->type() == SurfaceWrapper::Type::XWayland;
    bool isLayer = wrapper->type() == SurfaceWrapper::Type::Layer;

    connect(m_server->sessionManager()->activeSession().lock().get(),
            &Session::aboutToBeDestroyed,
            wrapper,
            &SurfaceWrapper::closeSurface);

    if (isXdgToplevel || isXdgPopup || isLayer) {
        auto *attached =
            new Personalization(wrapper->shellSurface(), m_personalizationInterfaceV1, wrapper);
        connect(wrapper, &SurfaceWrapper::aboutToBeInvalidated,
                attached, &Personalization::deleteLater);

        // New SSD personalization protocol (treeland-decoration-unstable-v1).
        // The frozen Personalization above handles the deprecated
        // treeland-personalization-manager-v1 window context; per-window
        // customization for the new protocol is applied via Decoration.
        auto *decoration =
            new Decoration(wrapper->shellSurface(), m_decorationInterfaceV1, wrapper);
        connect(wrapper, &SurfaceWrapper::aboutToBeInvalidated,
                decoration, &Decoration::deleteLater);

        // Single arbitration point for SurfaceWrapper::noTitleBar. Two
        // independent sources feed this: the frozen personalization protocol
        // (Personalization::noTitlebar) and the new per-window decoration
        // protocol (Decoration::titlebarHidden). Both write into the same
        // wrapper property, so the final value is decided here.
        auto updateNoTitlebar = [this, attached, decoration] {
            auto wrapper = attached->surfaceWrapper();
            const bool decorationHides = decoration->hasTitlebarOverride()
                && decoration->titlebarHidden();

            if (attached->noTitlebar() || decorationHides) {
                wrapper->setNoTitleBar(true);
                if (!wrapper->isLaunchpad()) {
                    wrapper->setNoDecoration(false);
                }
            } else {
                wrapper->setNoTitleBar(false);
                wrapper->setNoDecoration(m_xdgDecorationManager->modeBySurface(wrapper->surface())
                                     != WXdgDecorationManager::Server);
            }
        };

        if (isXdgToplevel) {
            connect(
                m_xdgDecorationManager,
                &WXdgDecorationManager::surfaceModeChanged,
                attached,
                [attached, updateNoTitlebar](
                    WAYLIB_SERVER_NAMESPACE::WSurface *surface,
                    [[maybe_unused]] Waylib::Server::WXdgDecorationManager::DecorationMode mode) {
                    if (surface == attached->surfaceWrapper()->surface()) {
                        updateNoTitlebar();
                    }
                });
        }

        connect(attached, &Personalization::windowStateChanged, this, updateNoTitlebar);
        // Re-arbitrate the titlebar whenever the decoration protocol changes
        // its override, so it can't be clobbered by (nor clobber) the
        // personalization path above.
        connect(decoration, &Decoration::titlebarOverrideChanged, this, updateNoTitlebar);
        updateNoTitlebar();

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
            attached->surfaceWrapper()->setRadius(attached->cornerRadius());
        };
        connect(attached, &Personalization::cornerRadiusChanged, this, updateCornerRadius);
        updateCornerRadius();
        updateBlur();
        if (wrapper->isLaunchpad())
            wrapper->setCoverEnabled(true);
    }

    if (isXwayland) {
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
        m_foreignToplevel->addSurface(wrapper->shellSurface());
        m_extForeignToplevelListV1->addSurface(wrapper->shellSurface());
    }
    connect(wrapper, &SurfaceWrapper::skipDockPreViewChanged, this, [this, wrapper] {
        if (wrapper->skipDockPreView()) {
            m_foreignToplevel->removeSurface(wrapper->shellSurface());
            m_extForeignToplevelListV1->removeSurface(wrapper->shellSurface());
        } else {
            m_foreignToplevel->addSurface(wrapper->shellSurface());
            m_extForeignToplevelListV1->addSurface(wrapper->shellSurface());
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
        m_foreignToplevel->removeSurface(wrapper->shellSurface());
        m_extForeignToplevelListV1->removeSurface(wrapper->shellSurface());
    }
    // Ensure the wrapper is removed from active history early to avoid cascading on half-invalid entries
    if (wrapper && wrapper->workspaceId() != -1) {
        auto ws = m_server->workspace();
        if (ws) {
            Q_ASSERT(ws == wrapper->container());
            ws->removeActivedSurface(wrapper);
        }
    }
}
