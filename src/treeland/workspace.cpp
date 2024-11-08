// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "workspace.h"

#include "helper.h"
#include "output.h"
#include "rootsurfacecontainer.h"
#include "surfacecontainer.h"
#include "surfacewrapper.h"
#include "treelandconfig.h"
#include "workspaceanimationcontroller.h"

Workspace::Workspace(SurfaceContainer *parent)
    : SurfaceContainer(parent)
    , m_currentIndex(TreelandConfig::ref().currentWorkspace())
    , m_models(new WorkspaceListModel(this))
    , m_currentFilter(new SurfaceFilterProxyModel(this))
    , m_animationController(new WorkspaceAnimationController(this))
{
    m_showOnAllWorkspaceModel = new WorkspaceModel(this, ShowOnAllWorkspaceId, {});
    m_showOnAllWorkspaceModel->setName("show-on-all-workspace");
    m_showOnAllWorkspaceModel->setVisible(true);
    for (auto index = 0; index < TreelandConfig::ref().numWorkspace(); index++) {
        doCreateModel(QStringLiteral("workspace-%1").arg(index),
                      index == TreelandConfig::ref().currentWorkspace());
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

void Workspace::moveSurfaceToNextWorkspace(SurfaceWrapper *surface)
{
    Q_ASSERT(surface);
    if (surface->showOnAllWorkspace() || surface->workspaceId() == -1)
        return;

    auto model = modelFromId(surface->workspaceId());
    Q_ASSERT(model);

    auto index = m_models->objects().indexOf(model);
    if (index == count() - 1)
        return;
    moveSurfaceTo(surface, modelAt(index + 1)->id());
}

void Workspace::moveSurfaceToPrevWorkspace(SurfaceWrapper *surface)
{
    Q_ASSERT(surface);
    if (surface->showOnAllWorkspace() || surface->workspaceId() == -1)
        return;

    auto model = modelFromId(surface->workspaceId());
    Q_ASSERT(model);

    auto index = m_models->objects().indexOf(model);
    if (index == 0)
        return;
    moveSurfaceTo(surface, modelAt(index - 1)->id());
}

void Workspace::addSurface(SurfaceWrapper *surface, int workspaceId)
{
    Q_ASSERT(surface && !surface->container() && surface->workspaceId() == -1);

    auto model = modelFromId(workspaceId);
    Q_ASSERT(model);

    model->addSurface(surface);

    if (!surface->ownsOutput())
        surface->setOwnsOutput(rootContainer()->primaryOutput());

    SurfaceContainer::addSurface(surface);
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
    SurfaceContainer::removeSurface(surface);

    WorkspaceModel *from = nullptr;
    if (surface->showOnAllWorkspace())
        from = m_showOnAllWorkspaceModel;
    else
        from = modelFromId(surface->workspaceId());
    Q_ASSERT(from);

    from->removeSurface(surface);
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
    TreelandConfig::ref().setNumWorkspace(TreelandConfig::ref().numWorkspace() + 1);
    Q_EMIT countChanged();
    return id;
}

void Workspace::removeModel(int index)
{
    Q_ASSERT(m_models->rowCount() >= 1); // At least one workspace
    Q_ASSERT(index >= 0 && index < m_models->rowCount());
    doRemoveModel(index);
    Q_EMIT countChanged();
    TreelandConfig::ref().setNumWorkspace(TreelandConfig::ref().numWorkspace() - 1);
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
    auto foundModel = std::find_if(m_models->objects().begin(),
                                   m_models->objects().end(),
                                   [id](WorkspaceModel *model) {
                                       return model->id() == id;
                                   });
    return foundModel == m_models->objects().end() ? nullptr : *foundModel;
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
    TreelandConfig::ref().setCurrentWorkspace(newCurrentIndex);
}

void Workspace::hideAllSurfacesExceptPreviewing(SurfaceWrapper *previewingItem)
{
    const auto &surfaceList = surfaces();
    for (auto surface : surfaceList) {
        surface->setOpacity(surface == previewingItem ? 1 : 0);
    }
}

void Workspace::showAllSurfaces()
{
    const auto &surfaceList = surfaces();
    for (auto surface : surfaceList) {
        surface->setOpacity(1);
    }
}

void Workspace::pushActivedSurface(SurfaceWrapper *surface)
{
    if (surface->showOnAllWorkspace()) [[unlikely]] {
        for (auto wpModle : m_models->objects())
            wpModle->pushActivedSurface(surface);
        m_showOnAllWorkspaceModel->pushActivedSurface(surface);
    } else {
        auto wpModle = modelFromId(surface->workspaceId());
        Q_ASSERT(wpModle);
        wpModle->pushActivedSurface(surface);
    }
}

void Workspace::removeActivedSurface(SurfaceWrapper *surface)
{
    if (surface->showOnAllWorkspace()) [[unlikely]] {
        for (auto wpModle : m_models->objects())
            wpModle->removeActivedSurface(surface);
        m_showOnAllWorkspaceModel->removeActivedSurface(surface);
    } else {
        auto wpModle = modelFromId(surface->workspaceId());
        Q_ASSERT(wpModle);
        wpModle->removeActivedSurface(surface);
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

    // TODO delete animation here
    const auto tmp = model->surfaces();
    for (auto s : tmp) {
        model->removeSurface(s);
        if (current)
            current->addSurface(s);
    }

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
        emit currentChanged();
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
