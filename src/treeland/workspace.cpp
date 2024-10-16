// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "workspace.h"

#include "helper.h"
#include "output.h"
#include "rootsurfacecontainer.h"
#include "surfacewrapper.h"
#include "treelandconfig.h"
#include "surfacecontainer.h"

Workspace::Workspace(SurfaceContainer *parent)
    : SurfaceContainer(parent)
    , m_currentIndex(TreelandConfig::ref().currentWorkspace())
    , m_models(new WorkspaceListModel(this))
{
    m_showOnAllWorkspaceModel = new WorkspaceModel(this, ShowOnAllWorkspaceIndex, {});
    m_showOnAllWorkspaceModel->setName("show-on-all-workspace");
    m_showOnAllWorkspaceModel->setVisible(true);
    for (auto index = 0; index < TreelandConfig::ref().numWorkspace(); index++) {
        doCreateModel(QStringLiteral("workspace-%1").arg(index), index == TreelandConfig::ref().currentWorkspace());
    }
}

void Workspace::moveSurfaceTo(SurfaceWrapper *surface, int workspaceIndex)
{
    if (workspaceIndex == -1)
        workspaceIndex = m_currentIndex;

    if (surface->workspaceId() == workspaceIndex)
        return;

    WorkspaceModel *from = nullptr;
    Q_ASSERT(surface->workspaceId() != -1);
    if (surface->showOnAllWorkspace())
        from = m_showOnAllWorkspaceModel;
    else
        from = model(surface->workspaceId());

    WorkspaceModel *to = nullptr;
    if (workspaceIndex == ShowOnAllWorkspaceIndex)
        to = m_showOnAllWorkspaceModel;
    else
        to = model(workspaceIndex);
    Q_ASSERT(to);

    from->removeSurface(surface);
    if (surface->shellSurface()->isActivated())
        Helper::instance()->activateSurface(current()->latestActiveSurface());

    to->addSurface(surface);
    if (surface->hasActiveCapability()
        && surface->shellSurface()->hasCapability(WToplevelSurface::Capability::Activate))
        pushActivedSurface(surface);
}

void Workspace::addSurface(SurfaceWrapper *surface, int workspaceIndex)
{
    Q_ASSERT(!surface->container() && surface->workspaceId() == -1);

    doAddSurface(surface, true);

    if (workspaceIndex < 0)
        workspaceIndex = m_currentIndex;

    auto model = this->model(workspaceIndex);
    model->addSurface(surface);

    if (!surface->ownsOutput())
        surface->setOwnsOutput(rootContainer()->primaryOutput());
}

void Workspace::moveModelTo(int workspaceIndex, int destinationIndex)
{

}

void Workspace::removeSurface(SurfaceWrapper *surface)
{
    if (!doRemoveSurface(surface, false))
        return;

    WorkspaceModel *from = nullptr;
    if (surface->showOnAllWorkspace())
        from = m_showOnAllWorkspaceModel;
    else
        from = model(surface->workspaceId());
    Q_ASSERT(from);

    from->removeSurface(surface);
}

int Workspace::modelIndexOfSurface(SurfaceWrapper *surface) const
{
    for (int i = 0; i < m_models->rowCount(); ++i) {
        if (model(i)->hasSurface(surface))
            return i;
    }

    if (m_showOnAllWorkspaceModel->hasSurface(surface))
        return ShowOnAllWorkspaceIndex;

    return -1;
}

int Workspace::createModel(const QString &name, bool visible)
{
    auto index = doCreateModel(name, visible);
    TreelandConfig::ref().setNumWorkspace(TreelandConfig::ref().numWorkspace() + 1);
    Q_EMIT countChanged();
    return index;
}

void Workspace::removeModel(int index)
{
    Q_ASSERT(m_models->rowCount() >= 1); // At least one workspace
    Q_ASSERT(index >= 0 && index < m_models->rowCount());
    doRemoveModel(index);
    Q_EMIT countChanged();
    TreelandConfig::ref().setNumWorkspace(TreelandConfig::ref().numWorkspace() - 1);
}

WorkspaceModel *Workspace::model(int index) const
{
    if (index < 0 || index >= m_models->rowCount())
        return nullptr;
    return m_models->objects().at(index);
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

    if (m_switcher) {
        m_switcher->deleteLater();
    }

    for (int i = 0; i < m_models->rowCount(); ++i) {
        model(i)->setVisible(i == currentIndex());
    }

    // Both changed
    Q_EMIT currentIndexChanged();
    Q_EMIT currentChanged();
}

void Workspace::switchToNext()
{
    if (currentIndex() < m_models->rowCount() - 1)
        switchTo(currentIndex() + 1);
}

void Workspace::switchToPrev()
{
    if (currentIndex() > 0)
        switchTo(currentIndex() - 1);
}

void Workspace::switchTo(int index)
{
    if (m_switcher)
        return;

    Q_ASSERT(index != currentIndex());
    Q_ASSERT(index >= 0 && index < m_models->rowCount());
    setCurrentIndex(index);
    Helper::instance()->activateSurface(current()->latestActiveSurface());

    // TODO new switch animation here
    // auto from = current();
    // auto to = model(index);
    // auto engine = Helper::instance()->qmlEngine();
    // from->setVisible(false);
    // to->setVisible(false);
    // m_switcher = engine->createWorkspaceSwitcher(this, from, to);
}

WorkspaceModel *Workspace::current() const
{
    return model(currentIndex());
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
        if (surface != previewingItem)
            surface->setOpacity(0);
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
        auto wpModle = model(surface->workspaceId());
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
        auto wpModle = model(surface->workspaceId());
        Q_ASSERT(wpModle);
        wpModle->removeActivedSurface(surface);
    }
}

int Workspace::doCreateModel(const QString &name, bool visible)
{
    auto newContainer = new WorkspaceModel(this, count(), m_showOnAllWorkspaceModel->m_activedSurfaceHistory);
    newContainer->setName(name);
    newContainer->setVisible(visible);
    m_models->addObject(newContainer);
    return newContainer->index();
}

void Workspace::doRemoveModel(int index)
{
    auto oldCurrent = this->current();
    auto oldCurrentIndex = this->currentIndex();
    auto model = this->model(index);
    m_models->removeObject(model);

    // reset index
    for (int i = index; i < count(); ++i) {
        this->model(i)->setIndex(i);
    }

    doSetCurrentIndex(std::min(currentIndex(), count() - 1));
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
        this->model(i)->setVisible(i == currentIndex());
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

void WorkspaceListModel::moveModelTo(int workspaceIndex, int destinationIndex)
{
    // Does not influence current but currentIndex
    if (workspaceIndex == destinationIndex)
        return;
    beginMoveRows({}, workspaceIndex, workspaceIndex, QModelIndex(), destinationIndex > workspaceIndex ? (destinationIndex + 1) : destinationIndex);
    m_objects.move(workspaceIndex, destinationIndex);
    endMoveRows();
}
