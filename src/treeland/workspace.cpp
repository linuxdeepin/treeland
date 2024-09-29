// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "workspace.h"

#include "helper.h"
#include "output.h"
#include "rootsurfacecontainer.h"
#include "surfacewrapper.h"

WorkspaceContainer::WorkspaceContainer(Workspace *parent, int index)
    : SurfaceContainer(parent)
    , m_index(index)
{
}

QString WorkspaceContainer::name() const
{
    return m_name;
}

void WorkspaceContainer::setName(const QString &newName)
{
    if (m_name == newName)
        return;
    m_name = newName;
    emit nameChanged();
}

Workspace::Workspace(SurfaceContainer *parent)
    : SurfaceContainer(parent)
{
    // TODO: save and restore from local storage
    static int workspaceGlobalIndex = 0;

    // TODO: save and restore workpsace's name from local storage
    createContainer(QStringLiteral("workspace-%1").arg(++workspaceGlobalIndex), true);
    createContainer(QStringLiteral("workspace-%1").arg(++workspaceGlobalIndex));
}

void Workspace::addSurface(SurfaceWrapper *surface, int workspaceIndex)
{
    doAddSurface(surface, false);
    auto container =
        workspaceIndex >= 0 ? m_containers.at(workspaceIndex) : m_containers.at(m_currentIndex);

    if (container->m_model->hasSurface(surface))
        return;

    for (auto c : std::as_const(m_containers)) {
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

    for (auto container : std::as_const(m_containers)) {
        if (container->surfaces().contains(surface)) {
            container->removeSurface(surface);
            break;
        }
    }
}

int Workspace::containerIndexOfSurface(SurfaceWrapper *surface) const
{
    for (int i = 0; i < m_containers.size(); ++i) {
        if (m_containers.at(i)->m_model->hasSurface(surface))
            return i;
    }

    return -1;
}

int Workspace::createContainer(const QString &name, bool visible)
{
    m_containers.append(new WorkspaceContainer(this, m_containers.size()));
    auto newContainer = m_containers.last();
    newContainer->setName(name);
    newContainer->setVisible(visible);
    return newContainer->index();
}

void Workspace::removeContainer(int index)
{
    if (m_containers.size() == 1)
        return;
    if (index < 0 || index >= m_containers.size())
        return;

    auto container = m_containers.at(index);
    m_containers.removeAt(index);

    // reset index
    for (int i = index; i < m_containers.size(); ++i) {
        m_containers.at(i)->setIndex(i);
    }

    auto oldCurrent = this->current();
    m_currentIndex = qMin(m_currentIndex, m_containers.size() - 1);
    auto current = this->current();

    const auto tmp = container->surfaces();
    for (auto s : tmp) {
        container->removeSurface(s);
        if (current)
            current->addSurface(s);
    }

    container->deleteLater();

    if (oldCurrent != current)
        emit currentChanged();
}

WorkspaceContainer *Workspace::container(int index) const
{
    if (index < 0 || index >= m_containers.size())
        return nullptr;
    return m_containers.at(index);
}

int Workspace::currentIndex() const
{
    return m_currentIndex;
}

void Workspace::setCurrentIndex(int newCurrentIndex)
{
    if (newCurrentIndex < 0 || newCurrentIndex >= m_containers.size())
        return;

    if (m_currentIndex == newCurrentIndex)
        return;
    m_currentIndex = newCurrentIndex;

    if (m_switcher) {
        m_switcher->deleteLater();
    }

    for (int i = 0; i < m_containers.size(); ++i) {
        m_containers.at(i)->setVisible(i == m_currentIndex);
    }

    emit currentChanged();
}

void Workspace::switchToNext()
{
    if (m_currentIndex < m_containers.size() - 1)
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
    Q_ASSERT(index >= 0 && index < m_containers.size());
    auto from = current();
    auto to = m_containers.at(index);
    auto engine = Helper::instance()->qmlEngine();
    from->setVisible(false);
    to->setVisible(false);
    m_switcher = engine->createWorkspaceSwitcher(this, from, to);
}

WorkspaceContainer *Workspace::current() const
{
    if (m_currentIndex < 0 || m_currentIndex >= m_containers.size())
        return nullptr;

    return m_containers.at(m_currentIndex);
}

void Workspace::setCurrent(WorkspaceContainer *container)
{
    int index = m_containers.indexOf(container);
    if (index < 0)
        return;
    setCurrentIndex(index);
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

int WorkspaceContainer::index() const
{
    return m_index;
}

void WorkspaceContainer::setIndex(int newIndex)
{
    if (m_index == newIndex)
        return;
    m_index = newIndex;
    emit indexChanged();
}
