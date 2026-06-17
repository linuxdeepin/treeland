// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcutrunner.h"
#include "seat/helper.h"
#include "common/treelandlogging.h"
#include "treelandconfig.hpp"
#include "shortcutcontroller.h"
#include "workspace/workspace.h"
#include "modules/window-management/windowmanagementinterfacev1.h"
#include "core/rootsurfacecontainer.h"
#include "surface/surfacewrapper.h"
#include "utils/fpsdisplaymanager.h"
#include "interfaces/multitaskviewinterface.h"
#include "core/lockscreen.h"
#include "core/qmlengine.h"
#include "workspaceanimationcontroller.h"
#include "woutputrenderwindow.h"
#include "output/output.h"

#include "qwayland-server-treeland-shortcut-manager-v2.h"

static bool isModifierKeyInMask(int key, Qt::KeyboardModifiers mods)
{
    if ((mods & Qt::AltModifier) && key == Qt::Key_Alt)
        return true;
    if ((mods & Qt::ControlModifier) && key == Qt::Key_Control)
        return true;
    if ((mods & Qt::MetaModifier) && key == Qt::Key_Meta)
        return true;
    if ((mods & Qt::ShiftModifier) && key == Qt::Key_Shift)
        return true;
    return false;
}

ShortcutRunner::ShortcutRunner(QObject *parent)
    : QObject(parent)
{
    auto *helper = Helper::instance();
    connect(helper, &Helper::modifierKeyReleased, this, &ShortcutRunner::onModifierReleased);

    m_quickSwitchTimer = new QTimer(this);
    m_quickSwitchTimer->setSingleShot(true);
    connect(m_quickSwitchTimer, &QTimer::timeout, this, &ShortcutRunner::onQuickSwitchTimeout);
}

void ShortcutRunner::onActionTrigger(ShortcutAction action, const QString &name, bool isGesture, ShortcutController::KeyFlags keyFlags)
{
    Q_UNUSED(isGesture)
    auto *helper = Helper::instance();

    if (helper->currentMode() == Helper::CurrentMode::LockScreen) {
        return;
    }

    m_currentAction = action;
    switch (action) {
    case ShortcutAction::Notify:
        helper->m_shortcutManager->sendActivated(name, keyFlags);
        break;
    case ShortcutAction::Workspace1:
        helper->restoreFromShowDesktop();
        helper->workspace()->switchTo(0);
        break;
    case ShortcutAction::Workspace2:
        helper->restoreFromShowDesktop();
        helper->workspace()->switchTo(1);
        break;
    case ShortcutAction::Workspace3:
        helper->restoreFromShowDesktop();
        helper->workspace()->switchTo(2);
        break;
    case ShortcutAction::Workspace4:
        helper->restoreFromShowDesktop();
        helper->workspace()->switchTo(3);
        break;
    case ShortcutAction::Workspace5:
        helper->restoreFromShowDesktop();
        helper->workspace()->switchTo(4);
        break;
    case ShortcutAction::Workspace6:
        helper->restoreFromShowDesktop();
        helper->workspace()->switchTo(5);
        break;
    case ShortcutAction::PrevWorkspace:
        helper->restoreFromShowDesktop();
        helper->workspace()->switchToPrev();
        break;
    case ShortcutAction::NextWorkspace:
        helper->restoreFromShowDesktop();
        helper->workspace()->switchToNext();
        break;
    case ShortcutAction::ShowDesktop:
        if (helper->currentMode() == Helper::CurrentMode::Multitaskview) {
            break;
        }
        if (helper->m_showDesktop == WindowManagementInterfaceV1::DesktopState::Normal)
            helper->m_windowManagementInterfaceV1->setDesktopState(WindowManagementInterfaceV1::DesktopState::Show);
        else if (helper->m_showDesktop == WindowManagementInterfaceV1::DesktopState::Show)
            helper->m_windowManagementInterfaceV1->setDesktopState(WindowManagementInterfaceV1::DesktopState::Normal);
        break;
    case ShortcutAction::Maximize: {
        auto surface = helper->activatedSurface();
        if (surface && surface->isMaximizable()) {
            surface->maximize();
        }
        break;
    }
    case ShortcutAction::CancelMaximize: {
        auto surface = helper->activatedSurface();
        if (surface) {
            surface->unmaximize();
        }
        break;
    }
    case ShortcutAction::MoveWindow: {
        auto surface = helper->activatedSurface();
        if (surface) {
            Q_EMIT surface->moveRequested();
        }
        break;
    }
    case ShortcutAction::CloseWindow: {
        auto surface = helper->activatedSurface();
        if (surface) {
            surface->closeSurface();
        }
        break;
    }
    case ShortcutAction::ShowWindowMenu:
        if (helper->m_activatedSurface) {
            Q_EMIT helper->m_activatedSurface->windowMenuRequested({ 0, 0 });
        }
        break;
    case ShortcutAction::OpenMultiTaskView:
        if (!helper->m_multitaskView ||
            (helper->currentMode() != Helper::CurrentMode::Normal
             && helper->currentMode() != Helper::CurrentMode::Multitaskview)) {
            break;
        }
        helper->m_multitaskView->setStatus(IMultitaskView::Exited);
        helper->m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::ShortcutKey);
        break;
    case ShortcutAction::CloseMultiTaskView:
        if (!helper->m_multitaskView ||
            (helper->currentMode() != Helper::CurrentMode::Normal
             && helper->currentMode() != Helper::CurrentMode::Multitaskview)) {
            break;
        }
        helper->m_multitaskView->setStatus(IMultitaskView::Active);
        helper->m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::ShortcutKey);
        break;
    case ShortcutAction::ToggleMultitaskView:
        if (helper->currentMode() == Helper::CurrentMode::Normal
            || helper->currentMode() == Helper::CurrentMode::Multitaskview) {
            helper->restoreFromShowDesktop();
            if (helper->m_multitaskView) {
                helper->m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::ShortcutKey);
            }
        }
        break;
    case ShortcutAction::ToggleFpsDisplay:
        helper->toggleFpsDisplay();
        break;
    case ShortcutAction::Lockscreen:
#ifndef DISABLE_DDM
        if (helper->m_lockScreen && helper->m_lockScreen->available() && helper->currentMode() == Helper::CurrentMode::Normal) {
            helper->showLockScreen();
        }
#endif
        break;
    case ShortcutAction::ShutdownMenu:
        if (helper->m_lockScreen && helper->m_lockScreen->available() && helper->currentMode() == Helper::CurrentMode::Normal) {
            helper->setCurrentMode(Helper::CurrentMode::LockScreen);
            helper->m_lockScreen->shutdown();
            helper->setWorkspaceVisible(false);
        }
        break;
    case ShortcutAction::Quit:
        Q_EMIT helper->requestQuit();
        break;
    case ShortcutAction::TaskSwitchNext:
    case ShortcutAction::TaskSwitchPrev:
    case ShortcutAction::TaskSwitchSameAppNext:
    case ShortcutAction::TaskSwitchSameAppPrev: {
        const bool isSameApp = (action == ShortcutAction::TaskSwitchSameAppNext || action == ShortcutAction::TaskSwitchSameAppPrev);
        const bool isPrev = (action == ShortcutAction::TaskSwitchPrev || action == ShortcutAction::TaskSwitchSameAppPrev);
        taskswitchAction(keyFlags.testFlag(ShortcutController::Repeat), isSameApp, isPrev);
        break;
    }
    default:
        break;
    }
}

void ShortcutRunner::onActionProgress(ShortcutAction action, qreal progress, const QString &name)
{
    Q_UNUSED(name);
    switch (action) {
    case ShortcutAction::PrevWorkspace:
        updateWorkspaceSwipe(-progress);
        break;
    case ShortcutAction::NextWorkspace:
        updateWorkspaceSwipe(progress);
        break;
    case ShortcutAction::OpenMultiTaskView:
    {
        auto helper = Helper::instance();
        if (helper->m_multitaskView)
            helper->m_multitaskView->updatePartialFactor(progress);
        break;
    }
    case ShortcutAction::CloseMultiTaskView:
    {
        auto helper = Helper::instance();
        if (helper->m_multitaskView)
            helper->m_multitaskView->updatePartialFactor(-progress);
        break;
    }
    default:
        break;
    }
}

void ShortcutRunner::onActionFinish(ShortcutAction action, const QString &name, bool isTriggered)
{
    Q_UNUSED(name);
    switch (action) {
    case ShortcutAction::PrevWorkspace:
    case ShortcutAction::NextWorkspace:
        finishWorkspaceSwipe();
        break;
    case ShortcutAction::OpenMultiTaskView:
    {
        auto helper = Helper::instance();
        if (!helper->m_multitaskView)
            break;
        helper->m_multitaskView->setStatus(IMultitaskView::Active);
        helper->m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::Gesture);
        break;
    }
    case ShortcutAction::CloseMultiTaskView:
    {
        auto helper = Helper::instance();
        if (!helper->m_multitaskView)
            break;
        helper->m_multitaskView->setStatus(IMultitaskView::Exited);
        helper->m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::Gesture);
        break;
    }
    default:
        if (isTriggered) {
            onActionTrigger(action, name, true, ShortcutController::KeyFlag::None);
        }
        break;
    }
}

void ShortcutRunner::updateWorkspaceSwipe(qreal cb)
{
    if (qFuzzyCompare(cb, m_desktopOffset))
        return;

    Workspace *workspace = Helper::instance()->workspace();
    Q_ASSERT(workspace);

    WorkspaceAnimationController *controller = workspace->animationController();
    Q_ASSERT(controller);

    m_desktopOffset = cb;
    if (!m_slideEnable) {
        m_slideEnable = true;
        m_slideBounce = false;

        m_fromId = workspace->currentIndex();
        if (cb > 0) {
            m_toId = m_fromId + 1;
            if (m_toId > workspace->count())
                return;

            if (m_toId == workspace->count())
                m_slideBounce = true;
        } else if (cb < 0) {
            m_toId = m_fromId - 1;
            if (m_toId < -1)
                return;

            if (m_toId == -1)
                m_slideBounce = true;
        }

        controller->slideNormal(m_fromId, m_toId);
        workspace->createSwitcher();
        controller->setRunning(true);
    }

    if (m_slideEnable) {
        controller->startGestureSlide(m_desktopOffset, m_slideBounce);
    }
}

void ShortcutRunner::finishWorkspaceSwipe()
{
    if (!m_slideEnable)
        return;

    m_slideEnable = false;

    // precision control. if it is infinitely close to 0, resetting the current index will cause
    // flickering
    qreal epison = std::floor(std::abs(m_desktopOffset) * 100) / 100;
    if (epison < 0.01)
        return;

    Workspace *workspace = Helper::instance()->workspace();
    if (!m_slideBounce && (m_desktopOffset > 0.98 || m_desktopOffset < -0.98)) {
        // m_desktopOffset is very close to 1 or -1, just set to the toId directly
        // Not need to play the slide animation
        workspace->setCurrentIndex(m_toId);
        auto *controller = workspace->animationController();
        controller->setRunning(false);
        return;
    }

    m_fromId = workspace->currentIndex();
    m_toId = 0;

    if (m_desktopOffset > 0.3) {
        m_toId = m_slideBounce ? m_fromId : m_fromId + 1;
        if (m_toId >= workspace->count())
            return;
    } else if (m_desktopOffset <= -0.3) {
        m_toId = m_slideBounce ? m_fromId : m_fromId - 1;
        if (m_toId < 0)
            return;
    } else {
        m_toId = m_fromId;
    }

    auto controller = workspace->animationController();
    if (m_toId >= 0 && m_toId < workspace->count()) {
        controller->slideRunning(m_toId);
        controller->startSlideAnimation();
        workspace->setCurrentIndex(m_toId);
    }
}

void ShortcutRunner::taskswitchAction(bool isRepeat, bool isSameApp, bool isPrev)
{
    auto *helper = Helper::instance();
    if (helper->currentMode() != Helper::CurrentMode::Normal && helper->currentMode() != Helper::CurrentMode::WindowSwitch)
        return;

    if (!isRepeat && helper->currentMode() == Helper::CurrentMode::Normal) {
        auto current = helper->workspace()->current();
        if (current) {
            auto nextSurface = current->findNextActivedSurface();
            if (nextSurface)
                helper->forceActivateSurface(nextSurface, Qt::TabFocusReason);
        }
        m_quickSwitchTimer->start(helper->globalConfig()->quickSwitchTimeout());
        m_quickSwitchPending = true;
        return;
    }

    if (helper->m_taskSwitch.isNull()) {
        auto contentItem = helper->window()->contentItem();
        auto output = helper->rootSurfaceContainer()->primaryOutput();
        helper->m_taskSwitch = helper->qmlEngine()->createTaskSwitcher(output, contentItem);
        helper->restoreFromShowDesktop();
        QObject::connect(helper->m_taskSwitch, SIGNAL(switchOnChanged()), helper, SLOT(deleteTaskSwitch()));
        helper->m_taskSwitch->setZ(RootSurfaceContainer::OverlayZOrder);
    }

    if (isRepeat) {
        m_taskAltCount++;
    } else {
        m_taskAltCount = 3;
    }

    if (m_taskAltCount < 3)
        return;

    m_taskAltCount = 0;
    helper->setCurrentMode(Helper::CurrentMode::WindowSwitch);

    QString appid;
    if (isSameApp) {
        auto surface = Helper::instance()->activatedSurface();
        if (surface) {
            appid = surface->shellSurface()->appId();
        }
    }
    auto filter = Helper::instance()->workspace()->currentFilter();
    filter->setFilterAppId(appid);
    if (isPrev) {
        QMetaObject::invokeMethod(helper->m_taskSwitch, "previous");
    } else {
        QMetaObject::invokeMethod(helper->m_taskSwitch, "next");
    }
    qCWarning(lcTlShortcut) << "TaskSwitch: navigating in full task switcher";
}

void ShortcutRunner::onQuickSwitchTimeout()
{
    m_quickSwitchPending = false;

    auto *helper = Helper::instance();
    if (helper->currentMode() != Helper::CurrentMode::Normal)
        return;

    if (helper->m_taskSwitch.isNull()) {
        auto contentItem = helper->window()->contentItem();
        auto output = helper->rootSurfaceContainer()->primaryOutput();
        helper->m_taskSwitch = helper->qmlEngine()->createTaskSwitcher(output, contentItem);
        helper->restoreFromShowDesktop();
        QObject::connect(helper->m_taskSwitch,
                         SIGNAL(switchOnChanged()),
                         helper,
                         SLOT(deleteTaskSwitch()));
        helper->m_taskSwitch->setZ(RootSurfaceContainer::OverlayZOrder);
    }
    helper->setCurrentMode(Helper::CurrentMode::WindowSwitch);

    QMetaObject::invokeMethod(helper->m_taskSwitch, "show");
}

void ShortcutRunner::onModifierReleased(QKeyEvent *event)
{
    auto *helper = Helper::instance();

    if (m_currentAction == ShortcutAction::TaskSwitchNext
        || m_currentAction == ShortcutAction::TaskSwitchPrev
        || m_currentAction == ShortcutAction::TaskSwitchSameAppNext
        || m_currentAction == ShortcutAction::TaskSwitchSameAppPrev) {
        auto modifiers =
            helper->m_shortcutManager->controller()->modifierForAction(m_currentAction);
        if (!isModifierKeyInMask(event->key(), modifiers))
            return;

        if (m_quickSwitchPending && helper->currentMode() == Helper::CurrentMode::Normal) {
            m_quickSwitchTimer->stop();
            m_quickSwitchPending = false;
            return;
        }

        if (helper->currentMode() == Helper::CurrentMode::WindowSwitch && helper->m_taskSwitch) {
            auto filter = helper->workspace()->currentFilter();
            filter->setFilterAppId("");
            helper->setCurrentMode(Helper::CurrentMode::Normal);
            QMetaObject::invokeMethod(helper->m_taskSwitch, "exit");
        }
    }
}
