// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "workspace.h"

#include "helper.h"
#include "output.h"
#include "rootsurfacecontainer.h"
#include "surfacewrapper.h"

Workspace::Workspace(SurfaceContainer *parent)
    : SurfaceContainer(parent)
{
    // TODO: save and restore from local storage
    static int workspaceGlobalIndex = 0;

    m_showOnAllWorkspaceModel = new WorkspaceModel(this, ShowOnAllWorkspaceIndex, {});
    m_showOnAllWorkspaceModel->setName("show-on-all-workspace");
    m_showOnAllWorkspaceModel->setVisible(true);
    // TODO: save and restore workspace's name from local storage
    createModel(QStringLiteral("workspace-%1").arg(++workspaceGlobalIndex), true);
    createModel(QStringLiteral("workspace-%1").arg(++workspaceGlobalIndex));
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
        Helper::instance()->activateSurface(current()->latestActivedSurface());

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

    auto model = m_models.at(workspaceIndex);
    model->addSurface(surface);

    if (!surface->ownsOutput())
        surface->setOwnsOutput(rootContainer()->primaryOutput());
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
    for (int i = 0; i < m_models.size(); ++i) {
        if (m_models.at(i)->hasSurface(surface))
            return i;
    }

    if (m_showOnAllWorkspaceModel->hasSurface(surface))
        return ShowOnAllWorkspaceIndex;

    return -1;
}

int Workspace::createModel(const QString &name, bool visible)
{
    m_models.append(new WorkspaceModel(this,
                                       m_models.size(),
                                       m_showOnAllWorkspaceModel->m_activedSurfaceHistory));
    auto newContainer = m_models.last();
    newContainer->setName(name);
    newContainer->setVisible(visible);

    return newContainer->index();
}

void Workspace::removeModel(int index)
{
    if (index < 0 || index >= m_models.size())
        return;

    auto model = m_models.at(index);
    m_models.removeAt(index);

    // reset index
    for (int i = index; i < m_models.size(); ++i) {
        m_models.at(i)->setIndex(i);
    }

    auto oldCurrent = this->current();
    m_currentIndex = qMin(m_currentIndex, m_models.size() - 1);
    auto current = this->current();

    const auto tmp = model->surfaces();
    for (auto s : tmp) {
        model->removeSurface(s);
        if (current)
            current->addSurface(s);
    }

    model->deleteLater();

    if (oldCurrent != current)
        Q_EMIT currentChanged();
}

WorkspaceModel *Workspace::model(int index) const
{
    if (index < 0 || index >= m_models.size())
        return nullptr;
    return m_models.at(index);
}

int Workspace::currentIndex() const
{
    return m_currentIndex;
}

void Workspace::setCurrentIndex(int newCurrentIndex)
{
    if (newCurrentIndex < 0 || newCurrentIndex >= m_models.size())
        return;

    if (m_currentIndex == newCurrentIndex)
        return;
    m_currentIndex = newCurrentIndex;

    if (m_switcher) {
        m_switcher->deleteLater();
    }

    for (int i = 0; i < m_models.size(); ++i) {
        m_models.at(i)->setVisible(i == m_currentIndex);
    }

    Q_EMIT currentChanged();
}

void Workspace::switchToNext()
{
    if (m_currentIndex < m_models.size() - 1)
        switchTo(m_currentIndex + 1);
}

void Workspace::switchToPrev()
{
    if (m_currentIndex > 0)
        switchTo(m_currentIndex - 1);
}

void Workspace::switchTo(int index)
{
    if (m_switcher)
        return;

    Q_ASSERT(index != m_currentIndex);
    Q_ASSERT(index >= 0 && index < m_models.size());
    auto from = current();
    auto to = m_models.at(index);
    auto engine = Helper::instance()->qmlEngine();
    from->setVisible(false);
    to->setVisible(false);
    m_switcher = engine->createWorkspaceSwitcher(this, from, to);
}

WorkspaceModel *Workspace::current() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_models.size())
        return nullptr;

    return m_models.at(m_currentIndex);
}

void Workspace::setCurrent(WorkspaceModel *model)
{
    int index = m_models.indexOf(model);
    if (index < 0)
        return;
    setCurrentIndex(index);
}

QQmlListProperty<WorkspaceModel> Workspace::models()
{
    return QQmlListProperty<WorkspaceModel>(this, &m_models);
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
    return m_models.size();
}

WorkspaceModel *Workspace::showOnAllWorkspaceModel() const
{
    return m_showOnAllWorkspaceModel;
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
        for (auto wpModle : m_models)
            wpModle->pushActivedSurface(surface);
        m_showOnAllWorkspaceModel->pushActivedSurface(surface);
    } else {
        auto wpModle = model(surface->workspaceId());
        wpModle->pushActivedSurface(surface);
    }
}

void Workspace::removeActivedSurface(SurfaceWrapper *surface)
{
    if (surface->showOnAllWorkspace()) [[unlikely]] {
        for (auto wpModle : m_models)
            wpModle->removeActivedSurface(surface);
        m_showOnAllWorkspaceModel->removeActivedSurface(surface);
    } else {
        auto wpModle = model(surface->workspaceId());
        wpModle->removeActivedSurface(surface);
    }
}
