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

    m_showOnAllWorkspaceModel = new WorkspaceModel(this, ShowOnAllWorkspaceIndex);
    m_showOnAllWorkspaceModel->setName("show-on-all-workspace");
    m_showOnAllWorkspaceModel->setVisible(true);
    // TODO: save and restore workpsace's name from local storage
    createModel(QStringLiteral("workspace-%1").arg(++workspaceGlobalIndex), true);
    createModel(QStringLiteral("workspace-%1").arg(++workspaceGlobalIndex));
}

void Workspace::addSurface(SurfaceWrapper *surface, int workspaceIndex)
{
    doAddSurface(surface, true);

    if (workspaceIndex < 0)
        workspaceIndex = m_currentIndex;

    auto container = m_models.at(workspaceIndex);

    if (container->hasSurface(surface))
        return;

    for (auto c : std::as_const(m_models)) {
        if (c == container)
            continue;
        if (c->surfaces().contains(surface)) {
            c->removeSurface(surface);
            break;
        }
    }

    container->addSurface(surface);
    if (!surface->ownsOutput())
        surface->setOwnsOutput(rootContainer()->primaryOutput());
}

void Workspace::removeSurface(SurfaceWrapper *surface)
{
    if (!doRemoveSurface(surface, false))
        return;

    for (auto container : std::as_const(m_models)) {
        if (container->surfaces().contains(surface)) {
            container->removeSurface(surface);
            break;
        }
    }
}

int Workspace::modelIndexOfSurface(SurfaceWrapper *surface) const
{
    for (int i = 0; i < m_models.size(); ++i) {
        if (m_models.at(i)->hasSurface(surface))
            return i;
    }

    return -1;
}

int Workspace::createModel(const QString &name, bool visible)
{
    m_models.append(new WorkspaceModel(this, m_models.size()));
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
        emit currentChanged();
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

    emit currentChanged();
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
