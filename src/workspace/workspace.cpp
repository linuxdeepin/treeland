// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "workspace.h"

#include "common/treelandlogging.h"
#include "core/rootsurfacecontainer.h"
#include "output/output.h"
#include "seat/helper.h"
#include "surface/surfacecontainer.h"
#include "surface/surfacewrapper.h"
#include "workspaceanimationcontroller.h"
#include "treelanduserconfig.hpp"

Workspace::Workspace(SurfaceContainer *parent)
    : SurfaceContainer(parent)
    , m_currentIndex(Helper::instance()->config()->currentWorkspace())
    , m_models(new WorkspaceListModel(this))
    , m_currentFilter(new SurfaceFilterProxyModel(this))
    , m_animationController(new WorkspaceAnimationController(this))
{
    m_showOnAllWorkspaceModel = new WorkspaceModel(this, ShowOnAllWorkspaceId, {});
    m_showOnAllWorkspaceModel->setName("show-on-all-workspace");
    m_showOnAllWorkspaceModel->setVisible(true);
    for (uint index = 0; index < Helper::instance()->config()->numWorkspace(); index++) {
        doCreateModel(QStringLiteral("workspace-%1").arg(index),
                      index == Helper::instance()->config()->currentWorkspace());
    }
}

void Workspace::moveSurfaceTo(SurfaceWrapper *surface, int workspaceId)
{
    if (workspaceId == -1)
        workspaceId = current()->id();

    Q_ASSERT(surface);
    if (surface->workspaceId() == workspaceId)
        return;

    WorkspaceModel *from = nullptr;
    Q_ASSERT(surface->workspaceId() != -1);
    if (surface->showOnAllWorkspace())
        from = m_showOnAllWorkspaceModel;
    else
        from = modelFromId(surface->workspaceId());

    WorkspaceModel *to = modelFromId(workspaceId);
    Q_ASSERT(to);

    from->removeSurface(surface);
    if (surface->shellSurface()->isActivated())
        Helper::instance()->activateSurface(current()->latestActiveSurface());

    to->addSurface(surface);
    if (surface->hasActiveCapability()
        && surface->shellSurface()->hasCapability(WToplevelSurface::Capability::Activate)) {
        if (surface->showOnWorkspace(current()->id())) {
            Helper::instance()->activateSurface(surface);
        } else {
            pushActivedSurface(surface);
        }
    }
}

int Workspace::getLeftWorkspaceId(int workspaceId)
{
    if (workspaceId == ShowOnAllWorkspaceId || workspaceId < 0)
        return -1;

    auto model = modelFromId(workspaceId);
    Q_ASSERT(model);

    auto index = m_models->objects().indexOf(model);
    if (index == 0)
        return -1;
    return modelAt(index - 1)->id();
}

int Workspace::getRightWorkspaceId(int workspaceId)
{
    if (workspaceId == ShowOnAllWorkspaceId || workspaceId < 0)
        return -1;

    auto model = modelFromId(workspaceId);
    Q_ASSERT(model);

    auto index = m_models->objects().indexOf(model);
    if (index == count() - 1)
        return -1;
    return modelAt(index + 1)->id();
}

void Workspace::addSurface(SurfaceWrapper *surface, int workspaceId)
{
    Q_ASSERT(surface && !surface->container() && surface->workspaceId() == -1);

    auto model = modelFromId(workspaceId);
    Q_ASSERT(model);

    SurfaceContainer::addSurface(surface);
    Q_ASSERT(surface->ownsOutput() || rootContainer()->primaryOutput() == nullptr);

    model->addSurface(surface);

    surface->setHasInitializeContainer(true);
}

void Workspace::addSurface(SurfaceWrapper *surface)
{
    addSurface(surface, current()->id());
}

void Workspace::moveModelTo(int workspaceId, int destinationIndex)
{
    // Current index might change, but current will not change
    auto oldCurrent = current();
    int fromIndex;
    for (fromIndex = 0; fromIndex < count(); ++fromIndex) {
        if (workspaceId == m_models->objects().at(fromIndex)->id()) {
            break;
        }
    }
    Q_ASSERT_X(fromIndex < count(), __func__, "Should pass a valid workspace id here.");
    m_models->moveRow({}, fromIndex, {}, destinationIndex);
    doSetCurrentIndex(m_models->objects().indexOf(oldCurrent));
    Q_EMIT currentIndexChanged();
}

void Workspace::removeSurface(SurfaceWrapper *surface)
{
    surface->setHasInitializeContainer(false);

    WorkspaceModel *from = nullptr;
    if (surface->showOnAllWorkspace())
        from = m_showOnAllWorkspaceModel;
    else
        from = modelFromId(surface->workspaceId());
    Q_ASSERT(from);

    from->removeSurface(surface);
    SurfaceContainer::removeSurface(surface);
}

int Workspace::modelIndexOfSurface(SurfaceWrapper *surface) const
{
    for (int i = 0; i < m_models->rowCount(); ++i) {
        if (modelAt(i)->hasSurface(surface))
            return i;
    }

    if (m_showOnAllWorkspaceModel->hasSurface(surface))
        return ShowOnAllWorkspaceId;

    return -1;
}

int Workspace::createModel(const QString &name, bool visible)
{
    auto id = doCreateModel(name, visible);
    Helper::instance()->config()->setNumWorkspace(count());
    Q_EMIT countChanged();
    return id;
}

void Workspace::removeModel(int index)
{
    Q_ASSERT(m_models->rowCount() > 1); // At least one workspace
    Q_ASSERT(index >= 0 && index < m_models->rowCount());
    doRemoveModel(index);
    Helper::instance()->config()->setNumWorkspace(count());
    Q_EMIT countChanged();
}

WorkspaceModel *Workspace::modelAt(int index) const
{
    if (index < 0 || index >= m_models->rowCount())
        return nullptr;
    return m_models->objects().at(index);
}

WorkspaceModel *Workspace::modelFromId(int id) const
{
    if (id == ShowOnAllWorkspaceId)
        return m_showOnAllWorkspaceModel;
    auto foundModel = std::find_if(m_models->objects().constBegin(),
                                   m_models->objects().constEnd(),
                                   [id](WorkspaceModel *model) {
                                       return model->id() == id;
                                   });
    return foundModel == m_models->objects().constEnd() ? nullptr : *foundModel;
}

int Workspace::currentIndex() const
{
    return m_currentIndex;
}

void Workspace::setCurrentIndex(int newCurrentIndex)
{
    if (newCurrentIndex < 0 || newCurrentIndex >= m_models->rowCount())
        return;

    if (currentIndex() == newCurrentIndex)
        return;

    doSetCurrentIndex(newCurrentIndex);

    for (int i = 0; i < m_models->rowCount(); ++i) {
        modelAt(i)->setVisible(i == currentIndex());
    }

    // Both changed
    Q_EMIT currentIndexChanged();
    Q_EMIT currentChanged();
}

void Workspace::switchToNext()
{
    if (currentIndex() < m_models->rowCount() - 1) {
        switchTo(currentIndex() + 1);
    } else {
        createSwitcher();
        m_animationController->bounce(currentIndex(), WorkspaceAnimationController::Right);
    }
}

void Workspace::switchToPrev()
{
    if (currentIndex() > 0) {
        switchTo(currentIndex() - 1);
    } else {
        createSwitcher();
        m_animationController->bounce(currentIndex(), WorkspaceAnimationController::Left);
    }
}

void Workspace::switchTo(int index)
{
    if (index < 0 || index >= m_models->rowCount() || index == currentIndex())
        return;
    auto oldCurrentIndex = currentIndex();
    setCurrentIndex(index);
    Helper::instance()->activateSurface(current()->latestActiveSurface());
    createSwitcher();
    m_animationController->slide(oldCurrentIndex, currentIndex());
}

WorkspaceModel *Workspace::current() const
{
    return modelAt(currentIndex());
}

SurfaceFilterProxyModel *Workspace::currentFilter()
{
    WorkspaceModel *wmodel = current();
    m_currentFilter->setSourceModel(wmodel);
    m_currentFilter->invalidate();
    m_currentFilter->sort(0);
    return m_currentFilter;
}

void Workspace::setCurrent(WorkspaceModel *model)
{
    int index = m_models->objects().indexOf(model);
    if (index < 0)
        return;
    setCurrentIndex(index);
}

WorkspaceListModel *Workspace::models()
{
    return m_models;
}

void Workspace::updateSurfaceOwnsOutput(SurfaceWrapper *surface)
{
    auto outputs = surface->surface()->outputs();
    if (surface->ownsOutput() && outputs.contains(surface->ownsOutput()->output()))
        return;

    Output *output = nullptr;
    if (!outputs.isEmpty())
        output = Helper::instance()->getOutput(outputs.first());
    if (!output)
        output = rootContainer()->cursorOutput();
    if (!output)
        output = rootContainer()->primaryOutput();
    if (output)
        surface->setOwnsOutput(output);
}

void Workspace::updateSurfacesOwnsOutput()
{
    const auto surfaces = this->surfaces();
    for (auto surface : surfaces) {
        updateSurfaceOwnsOutput(surface);
    }
}

void Workspace::createSwitcher()
{
    if (m_switcherEnabled && !m_switcher) {
        auto engine = Helper::instance()->qmlEngine();
        m_switcher = engine->createWorkspaceSwitcher(this);
        connect(m_switcher, &QQuickItem::visibleChanged, m_switcher, [this] {
            if (!m_switcher->isVisible()) {
                m_switcher->deleteLater();
            }
        });
    }
}

int Workspace::count() const
{
    return m_models->rowCount();
}

WorkspaceModel *Workspace::showOnAllWorkspaceModel() const
{
    return m_showOnAllWorkspaceModel;
}

void Workspace::doSetCurrentIndex(int newCurrentIndex)
{
    m_currentIndex = newCurrentIndex;
    Helper::instance()->config()->setCurrentWorkspace(newCurrentIndex);
}

void Workspace::startPreviewing(SurfaceWrapper *previewingItem)
{
    if (m_previewingItem && m_previewingItem->shellSurface()) {
        // Check shellSurface() since SurfaceWrapper::aboutToBeInvalidated can't make QPointer null
        // in time
        auto model = modelFromId(m_previewingItem->workspaceId());
        m_previewingItem->setOpacity(model->opaque() ? 1.0 : 0.0);
        m_previewingItem->setHideByWorkspace(!model->visible());
    }
    m_previewingItem = previewingItem;
    current()->setOpaque(false);
    previewingItem->setOpacity(1);
    previewingItem->setHideByWorkspace(false);
}

void Workspace::stopPreviewing()
{
    current()->setOpaque(true);
    if (m_previewingItem && m_previewingItem->shellSurface()) {
        auto model = modelFromId(m_previewingItem->workspaceId());
        m_previewingItem->setOpacity(model->opaque() ? 1.0 : 0.0);
        m_previewingItem->setHideByWorkspace(!model->visible());
        m_previewingItem = nullptr;
    }
}

void Workspace::pushActivedSurface(SurfaceWrapper *surface)
{
    if (surface->type() == SurfaceWrapper::Type::XdgPopup) {
        qWarning(treelandWorkspace) << "XdgPopup can't participate in focus fallback!";
        return;
    }
    if (surface->showOnAllWorkspace()) [[unlikely]] {
        for (auto wpModel : m_models->objects())
            wpModel->pushActivedSurface(surface);
        m_showOnAllWorkspaceModel->pushActivedSurface(surface);
    } else {
        auto wpModel = modelFromId(surface->workspaceId());
        Q_ASSERT(wpModel);
        wpModel->pushActivedSurface(surface);
    }
}

void Workspace::removeActivedSurface(SurfaceWrapper *surface)
{
    if (surface->showOnAllWorkspace()) [[unlikely]] {
        for (auto wpModel : m_models->objects())
            wpModel->removeActivedSurface(surface);
        m_showOnAllWorkspaceModel->removeActivedSurface(surface);
    } else {
        auto wpModel = modelFromId(surface->workspaceId());
        Q_ASSERT(wpModel);
        wpModel->removeActivedSurface(surface);
    }
}

void Workspace::setSwitcherEnabled(bool enabled)
{
    m_switcherEnabled = enabled;
}

int Workspace::nextWorkspaceId() const
{
    static int globalWorkspaceId = 0;
    return globalWorkspaceId++;
}

int Workspace::doCreateModel(const QString &name, bool visible)
{
    auto newContainer = new WorkspaceModel(this,
                                           nextWorkspaceId(),
                                           m_showOnAllWorkspaceModel->m_activedSurfaceHistory);
    if (name.isEmpty()) {
        newContainer->setName(QString("workspace-%1").arg(newContainer->id()));
    } else {
        newContainer->setName(name);
    }
    newContainer->setVisible(visible);
    m_models->addObject(newContainer);
    return newContainer->id();
}

void Workspace::doRemoveModel(int index)
{
    auto oldCurrent = this->current();
    auto oldCurrentIndex = this->currentIndex();
    auto model = this->modelAt(index);
    m_models->removeObject(model);

    if (oldCurrent == model) {
        // current change, current index might change
        doSetCurrentIndex(std::min(currentIndex(), count() - 1));
    } else {
        // current do not change, current index might change
        doSetCurrentIndex(m_models->objects().indexOf(oldCurrent));
    }

    auto current = this->current();
    Q_ASSERT(current);

    // TODO delete animation here
    const auto tmp = model->surfaces();
    for (auto s : tmp) {
        model->removeSurface(s);
        current->addSurface(s);
        if (s->hasActiveCapability() && !s->showOnAllWorkspace())
            current->pushActivedSurface(s);
    }
    Helper::instance()->activateSurface(current->latestActiveSurface());

    if (m_switcher) {
        m_switcher->deleteLater();
    }

    for (int i = 0; i < count(); ++i) {
        this->modelAt(i)->setVisible(i == currentIndex());
    }

    model->deleteLater();

    if (oldCurrentIndex != currentIndex())
        Q_EMIT currentIndexChanged();
    if (oldCurrent != current)
        Q_EMIT currentChanged();
}

WorkspaceListModel::WorkspaceListModel(QObject *parent)
    : ObjectListModel<WorkspaceModel>("workspace", parent)
{
}

WorkspaceAnimationController *Workspace::animationController() const
{
    return m_animationController;
}

bool WorkspaceListModel::moveRows(const QModelIndex &sourceParent,
                                  int sourceRow,
                                  int count,
                                  const QModelIndex &destinationParent,
                                  int destinationChild)
{
    // Do not support complex move
    if (sourceParent.isValid() || destinationParent.isValid() || count != 1)
        return false;
    auto beginSuccess =
        beginMoveRows({},
                      sourceRow,
                      sourceRow,
                      {},
                      destinationChild > sourceRow ? (destinationChild + 1) : destinationChild);
    if (!beginSuccess)
        return false;
    m_objects.move(sourceRow, destinationChild);
    endMoveRows();
    return true;
}
