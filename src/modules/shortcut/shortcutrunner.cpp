// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "shortcutrunner.h"

#include "common/shellaction.h"
#include "core/qmlengine.h"
#include "core/rootsurfacecontainer.h"
#include "interfaces/multitaskviewinterface.h"
#include "seat/helper.h"
#include "shortcutcontroller.h"
#include "treelandconfig.hpp"
#include "workspace/workspace.h"
#include "workspaceanimationcontroller.h"
#include "woutputrenderwindow.h"

#include <optional>

// Maps the shortcut-manager action enum (ShortcutAction) onto the shared
// ShellAction vocabulary. Notify, Quit and the task-switch stepping actions
// are producer-local and return std::nullopt.
static std::optional<ShellAction> mapShortcutAction(ShortcutAction action)
{
    switch (action) {
    case ShortcutAction::Workspace1:
        return ShellAction::SwitchWorkspace1;
    case ShortcutAction::Workspace2:
        return ShellAction::SwitchWorkspace2;
    case ShortcutAction::Workspace3:
        return ShellAction::SwitchWorkspace3;
    case ShortcutAction::Workspace4:
        return ShellAction::SwitchWorkspace4;
    case ShortcutAction::Workspace5:
        return ShellAction::SwitchWorkspace5;
    case ShortcutAction::Workspace6:
        return ShellAction::SwitchWorkspace6;
    case ShortcutAction::PrevWorkspace:
        return ShellAction::PreviousWorkspace;
    case ShortcutAction::NextWorkspace:
        return ShellAction::NextWorkspace;
    case ShortcutAction::ShowDesktop:
        return ShellAction::ToggleShowDesktop;
    case ShortcutAction::OpenMultiTaskView:
        return ShellAction::OpenMultitaskView;
    case ShortcutAction::CloseMultiTaskView:
        return ShellAction::CloseMultitaskView;
    case ShortcutAction::ToggleMultitaskView:
        return ShellAction::ToggleMultitaskView;
    case ShortcutAction::ToggleFpsDisplay:
        return ShellAction::ToggleFpsDisplay;
    case ShortcutAction::Lockscreen:
        return ShellAction::LockScreen;
    case ShortcutAction::ShutdownMenu:
        return ShellAction::ShowShutdownMenu;
    case ShortcutAction::Maximize:
        return ShellAction::Maximize;
    case ShortcutAction::CancelMaximize:
        return ShellAction::CancelMaximize;
    case ShortcutAction::MoveWindow:
        return ShellAction::MoveWindow;
    case ShortcutAction::CloseWindow:
        return ShellAction::CloseWindow;
    case ShortcutAction::ShowWindowMenu:
        return ShellAction::ShowWindowMenu;
    case ShortcutAction::TileLeft:
        return ShellAction::TileLeft;
    case ShortcutAction::TileRight:
        return ShellAction::TileRight;
    default:
        return std::nullopt;
    }
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

    if (const auto shellAction = mapShortcutAction(action)) {
        ShellActionExecutor::execute(*shellAction);
        return;
    }

    switch (action) {
    case ShortcutAction::Notify:
        helper->m_shortcutManager->sendActivated(name, keyFlags);
        break;
    case ShortcutAction::Quit:
        Q_EMIT helper->requestQuit();
        break;
    case ShortcutAction::TaskSwitchNext:
    case ShortcutAction::TaskSwitchPrev:
    case ShortcutAction::TaskSwitchSameAppNext:
    case ShortcutAction::TaskSwitchSameAppPrev: {
        if (keyFlags.testFlag(ShortcutController::Repeat)
            && helper->currentMode() == Helper::CurrentMode::Normal
            && !m_quickSwitchPending) {
            return;
        }

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
        if (!helper->m_multitaskView || !helper->isNormalOrMultitaskview())
            break;
        if (helper->currentMode() == Helper::CurrentMode::Normal
            && qFuzzyIsNull(helper->m_multitaskView->partialFactor())) {
            helper->m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::Gesture);
        }
        if (helper->currentMode() == Helper::CurrentMode::Multitaskview) {
            break;
        }
        helper->m_multitaskView->updatePartialFactor(progress);
        break;
    }
    case ShortcutAction::CloseMultiTaskView:
    {
        auto helper = Helper::instance();
        if (!helper->m_multitaskView || !helper->isNormalOrMultitaskview())
            break;
        if (helper->currentMode() != Helper::CurrentMode::Multitaskview) {
            break;
        }
        helper->m_multitaskView->updatePartialFactor(1 - progress);
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
        if (!helper->m_multitaskView || !helper->isNormalOrMultitaskview())
            break;
        const bool triggered = helper->m_multitaskView->partialFactor() > 0.5;
        helper->m_multitaskView->setStatus(triggered ? IMultitaskView::Active
                                                     : IMultitaskView::Exited);
        helper->m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::Gesture);
        helper->m_multitaskView->commitGesture(triggered);
        break;
    }
    case ShortcutAction::CloseMultiTaskView:
    {
        auto helper = Helper::instance();
        if (!helper->m_multitaskView || !helper->isNormalOrMultitaskview())
            break;
        const bool triggered = (1.0 - helper->m_multitaskView->partialFactor()) > 0.5;
        if (qFuzzyCompare(helper->m_multitaskView->partialFactor(), 0.0)
            && helper->currentMode() != Helper::CurrentMode::Multitaskview)
            break;
        helper->m_multitaskView->setStatus(triggered ? IMultitaskView::Exited
                                                     : IMultitaskView::Active);
        helper->m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::Gesture);
        helper->m_multitaskView->commitGesture(!triggered);
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
        m_fromId = workspace->currentIndex();
        controller->slideNormal(m_fromId);
        workspace->createSwitcher();
        controller->setRunning(true);
    }

    controller->startGestureSlide(
        cb,
        isWorkspaceBounce(cb, workspace->currentIndex(), workspace->count()));
}

void ShortcutRunner::finishWorkspaceSwipe()
{
    if (!m_slideEnable)
        return;
    m_slideEnable = false;
    Workspace *workspace = Helper::instance()->workspace();
    int currentIdx = workspace->currentIndex();
    int wsCount = workspace->count();

    // determine target based on offset threshold, but never cross boundary
    m_toId = currentIdx;
    if (m_desktopOffset > 0.3) {
        if (!isWorkspaceBounce(m_desktopOffset, currentIdx, wsCount))
            m_toId = currentIdx + 1;
    } else if (m_desktopOffset <= -0.3) {
        if (!isWorkspaceBounce(m_desktopOffset, currentIdx, wsCount))
            m_toId = currentIdx - 1;
    }

    // final safety bounds check
    m_toId = qBound(0, m_toId, wsCount - 1);

    auto controller = workspace->animationController();
    controller->slideRunning(m_toId);
    controller->startSlideAnimation();
    workspace->switchTo(m_toId, false);
}
void ShortcutRunner::taskswitchAction(bool isRepeat, bool isSameApp, bool isPrev)
{
    auto *helper = Helper::instance();
    const auto modeBefore = helper->currentMode();
    if (modeBefore != Helper::CurrentMode::Normal && modeBefore != Helper::CurrentMode::WindowSwitch)
        return;

    if (!isRepeat && !isSameApp && modeBefore == Helper::CurrentMode::Normal) {
        auto *current = helper->workspace()->current();
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
        auto output = helper->rootSurfaceContainer()->cursorOutput();
        if (!output) {
            output = helper->rootSurfaceContainer()->primaryOutput();
        }
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

    if (m_quickSwitchPending) {
        m_quickSwitchTimer->stop();
        m_quickSwitchPending = false;
    }

    if (isSameApp) {
        QMetaObject::invokeMethod(helper->m_taskSwitch, isPrev ? "previousSameApp" : "nextSameApp");
    } else {
        auto filter = helper->workspace()->currentFilter();
        filter->setFilterAppId({});
        QMetaObject::invokeMethod(helper->m_taskSwitch, isPrev ? "previous" : "next");
    }
}

void ShortcutRunner::onQuickSwitchTimeout()
{
    auto *helper = Helper::instance();
    if (helper->currentMode() != Helper::CurrentMode::Normal) {
        m_quickSwitchPending = false;
        return;
    }

    if (helper->m_taskSwitch.isNull()) {
        auto contentItem = helper->window()->contentItem();
        auto output = helper->rootSurfaceContainer()->cursorOutput();
        if (!output) {
            output = helper->rootSurfaceContainer()->primaryOutput();
        }
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

    const QList<ShortcutAction> taskSwitchActions = {
        ShortcutAction::TaskSwitchNext,
        ShortcutAction::TaskSwitchPrev,
        ShortcutAction::TaskSwitchSameAppNext,
        ShortcutAction::TaskSwitchSameAppPrev,
    };

    if (taskSwitchActions.contains(m_currentAction)) {
        auto *controller = helper->m_shortcutManager->controller();
        const auto currentModifiers = controller->modifierForAction(m_currentAction);

        constexpr auto supportedModifiers =
            Qt::AltModifier | Qt::MetaModifier | Qt::ControlModifier | Qt::ShiftModifier;

        auto keyCombination = QKeyCombination(Qt::NoModifier,static_cast<Qt::Key>(event->key()));
        const auto releasedModifier = ShortcutController::normalizeKeyCombination(keyCombination).keyboardModifiers() & supportedModifiers;

        if (!(currentModifiers & releasedModifier))
            return;

        auto remainingModifiers = event->modifiers() & supportedModifiers;
        remainingModifiers &= ~releasedModifier;

        const bool hasActiveTaskSwitchModifiers =
            std::any_of(taskSwitchActions.cbegin(), taskSwitchActions.cend(), [
                controller,
                currentModifiers,
                remainingModifiers](auto action) {
            const auto modifiers = controller->modifierForAction(action);
            if (modifiers == Qt::NoModifier)
                return false;
            if ((modifiers & currentModifiers) != modifiers)
                return false;
            return (remainingModifiers & modifiers) == modifiers;
        });
        if (hasActiveTaskSwitchModifiers)
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
            m_quickSwitchPending = false;
            QMetaObject::invokeMethod(helper->m_taskSwitch, "exit");
        }
    }
}
