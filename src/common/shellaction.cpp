// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shellaction.h"

#include "common/treelandlogging.h"
#include "interfaces/multitaskviewinterface.h"
#include "modules/show-desktop/showdesktopinterfacev1.h"
#include "output/output.h"
#include "seat/helper.h"
#include "surface/surfacewrapper.h"
#include "workspace/workspace.h"

void ShellActionExecutor::execute(ShellAction action)
{
    auto *helper = Helper::instance();

    if (helper->currentMode() == Helper::CurrentMode::LockScreen) {
        return;
    }

    switch (action) {
    case ShellAction::Invalid:
        break;
    case ShellAction::SwitchWorkspace1:
    case ShellAction::SwitchWorkspace2:
    case ShellAction::SwitchWorkspace3:
    case ShellAction::SwitchWorkspace4:
    case ShellAction::SwitchWorkspace5:
    case ShellAction::SwitchWorkspace6:
    case ShellAction::SwitchWorkspace7:
    case ShellAction::SwitchWorkspace8:
    case ShellAction::SwitchWorkspace9:
    case ShellAction::SwitchWorkspace10:
    case ShellAction::SwitchWorkspace11:
    case ShellAction::SwitchWorkspace12:
        // switchTo() ignores out-of-range indexes, as the protocol requires.
        helper->restoreFromShowDesktop();
        helper->workspace()->switchTo(static_cast<int>(action) - static_cast<int>(ShellAction::SwitchWorkspace1));
        break;
    case ShellAction::PreviousWorkspace:
        helper->restoreFromShowDesktop();
        helper->workspace()->switchToPrev();
        break;
    case ShellAction::NextWorkspace:
        helper->restoreFromShowDesktop();
        helper->workspace()->switchToNext();
        break;
    case ShellAction::ToggleShowDesktop:
        if (!helper->m_showDesktopInterfaceV1
            || helper->currentMode() == Helper::CurrentMode::Multitaskview) {
            break;
        }
        if (helper->showDesktopState() == ShowDesktopInterfaceV1::State::Normal) {
            helper->m_showDesktopInterfaceV1->setDesktopState(ShowDesktopInterfaceV1::State::Show);
        } else if (helper->showDesktopState() == ShowDesktopInterfaceV1::State::Show) {
            helper->m_showDesktopInterfaceV1->setDesktopState(ShowDesktopInterfaceV1::State::Normal);
        }
        break;
    case ShellAction::OpenMultitaskView:
        if (!helper->m_multitaskView || !helper->isNormalOrMultitaskview()
            || helper->currentMode() == Helper::CurrentMode::Multitaskview) {
            break;
        }
        helper->m_multitaskView->setStatus(IMultitaskView::Exited);
        helper->m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::ShortcutKey);
        break;
    case ShellAction::CloseMultitaskView:
        if (!helper->m_multitaskView || !helper->isNormalOrMultitaskview()
            || helper->currentMode() == Helper::CurrentMode::Normal) {
            break;
        }
        helper->m_multitaskView->setStatus(IMultitaskView::Active);
        helper->m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::ShortcutKey);
        break;
    case ShellAction::ToggleMultitaskView:
        if (!helper->m_multitaskView || !helper->isNormalOrMultitaskview()) {
            break;
        }
        helper->restoreFromShowDesktop();
        helper->m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::ShortcutKey);
        break;
    case ShellAction::ToggleFpsDisplay:
        helper->toggleFpsDisplay();
        break;
    case ShellAction::ZoomIn:
    case ShellAction::ZoomOut:
    case ShellAction::ZoomReset:
        // TODO: implement screen zoom.
        break;
    case ShellAction::LockScreen:
        // Rejects the transient WindowSwitch mode, where locking would strand
        // the task-switch sequence.
        if (helper->isNormalOrMultitaskview())
            helper->showLockScreen();
        break;
    case ShellAction::ShowShutdownMenu:
        helper->showShutdownMenu();
        break;
    case ShellAction::ShowShutdownMenuPowerOff:
    case ShellAction::ShowShutdownMenuReboot:
    case ShellAction::ShowShutdownMenuSuspend:
    case ShellAction::ShowShutdownMenuHibernate:
    case ShellAction::ShowShutdownMenuLogOut:
        // TODO: implement default-focus menu variants.
        qCWarning(lcTlShell) << "Default-focus shutdown menu variants not implemented;"
                             << "falling back to the plain shutdown menu";
        helper->showShutdownMenu();
        break;
    case ShellAction::ShowUserSwitch:
        helper->showSwitchUser();
        break;
    case ShellAction::Maximize: {
        auto *surface = helper->activatedSurface();
        if (surface && surface->isMaximizable()) {
            surface->maximize();
        }
        break;
    }
    case ShellAction::CancelMaximize: {
        auto *surface = helper->activatedSurface();
        if (surface) {
            surface->unmaximize();
        }
        break;
    }
    case ShellAction::MoveWindow: {
        auto *surface = helper->activatedSurface();
        if (surface) {
            Q_EMIT surface->moveRequested();
        }
        break;
    }
    case ShellAction::CloseWindow: {
        auto *surface = helper->activatedSurface();
        if (surface) {
            surface->closeSurface();
        }
        break;
    }
    case ShellAction::ShowWindowMenu: {
        auto *surface = helper->activatedSurface();
        if (surface) {
            Q_EMIT surface->windowMenuRequested({ 0, 0 });
        }
        break;
    }
    case ShellAction::TileLeft:
    case ShellAction::TileRight: {
        auto *surface = helper->activatedSurface();
        if (!surface) {
            break;
        }
        auto *output = surface->ownsOutput();
        if (!output) {
            break;
        }
        const auto mode = (action == ShellAction::TileLeft) ? SurfaceWrapper::TileMode::Left
                                                            : SurfaceWrapper::TileMode::Right;
        surface->applyTileMode(mode, output);
        break;
    }
    }
}
