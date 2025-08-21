// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "surfacecontainer.h"
#include "output.h"
#include "rootsurfacecontainer.h"

SurfaceListModel::SurfaceListModel(QObject *parent)
    : ObjectListModel("surface", parent)
{

}

QHash<int, QByteArray> SurfaceListModel::roleNames() const
{
    auto roleMap = ObjectListModel::roleNames();
    roleMap.insert(Qt::InitialSortOrderRole, "orderIndex");

    return roleMap;
}

QMap<int, QVariant> SurfaceListModel::itemData(const QModelIndex &index) const
{
    auto datas = ObjectListModel::itemData(index);
    if (datas.isEmpty())
        return datas;

    auto surface = surfaces().at(index.row());
    auto container = surface->container();
    const auto orderIndex = container->childItems().indexOf(surface);
    Q_ASSERT(orderIndex >= 0);
    datas.insert(Qt::InitialSortOrderRole, orderIndex);

    return datas;
}

QVariant SurfaceListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_objects.count())
        return {};

    if (role == Qt::InitialSortOrderRole) {
        auto surface = surfaces().at(index.row());
        auto container = surface->container();
        const auto orderIndex = container->childItems().indexOf(surface);
        Q_ASSERT(orderIndex >= 0);
        return orderIndex;
    }

    return ObjectListModel::data(index, role);
}

SurfaceFilterModel::SurfaceFilterModel(SurfaceListModel *parent)
    : SurfaceListModel(parent)
{
    connect(parent, &SurfaceListModel::surfaceAdded,
            this, &SurfaceFilterModel::initForSourceSurface);
    connect(parent, &SurfaceListModel::surfaceRemoved,
            this, [this] (SurfaceWrapper *surface) {
        removeSurface(surface);
        m_properties.erase(surface);
    });
}

void SurfaceFilterModel::setFilter(std::function<bool(SurfaceWrapper*)> filter)
{
    m_filter = filter;
    m_properties.clear();

    for (auto surface : parent()->surfaces()) {
        initForSourceSurface(surface);
    }
}

void SurfaceFilterModel::initForSourceSurface(SurfaceWrapper *surface)
{
    if (m_filter) {
        makeBindingForSourceSurface(surface);
    }
    updateSurfaceVisibility(surface);
}

void SurfaceFilterModel::makeBindingForSourceSurface(SurfaceWrapper *surface)
{
    Property &p = m_properties[surface];

    if (!p.init) {
        p.init = true;
        p.notifier = p.prop.addNotifier([this, surface] {
            updateSurfaceVisibility(surface);
        });
    }

    p.setBinding([this, surface] {
        return m_filter(surface);
    });
}

void SurfaceFilterModel::updateSurfaceVisibility(SurfaceWrapper *surface)
{
    if (!m_filter) {
        if (hasSurface(surface))
            return;
        addSurface(surface);
        return;
    }

    Q_ASSERT(m_properties.contains(surface));
    Q_ASSERT(parent()->hasSurface(surface));
    const Property &p = m_properties.at(surface);
    Q_ASSERT(p.init);
    if (p.prop.value()) {
        addSurface(surface);
    } else {
        removeSurface(surface);
    }
}

SurfaceContainer::SurfaceContainer(QQuickItem *parent)
    : QQuickItem(parent)
    , m_model(new SurfaceListModel(this))
{

}

SurfaceContainer::SurfaceContainer(SurfaceContainer *parent)
    : SurfaceContainer(static_cast<QQuickItem*>(parent))
{

}

SurfaceContainer::~SurfaceContainer()
{
    if (!m_model->surfaces().isEmpty()) {
        qWarning() << "SurfaceContainer destroyed with surfaces still attached:" << m_model->surfaces();
    }
}

RootSurfaceContainer *SurfaceContainer::rootContainer() const
{
    SurfaceContainer *root = const_cast<SurfaceContainer*>(this);
    while (auto p = root->parentContainer()) {
        root = p;
    }

    auto r = qobject_cast<RootSurfaceContainer*>(root);
    Q_ASSERT(r);
    return r;
}

SurfaceContainer *SurfaceContainer::parentContainer() const
{
    return qobject_cast<SurfaceContainer*>(parent());
}

QList<SurfaceContainer*> SurfaceContainer::subContainers() const
{
    return findChildren<SurfaceContainer*>(Qt::FindDirectChildrenOnly);
}

void SurfaceContainer::setQmlEngine(QQmlEngine *engine)
{
    engine->setContextForObject(this, engine->rootContext());

    const auto subContainers = this->subContainers();
    for (auto sub : subContainers) {
        sub->setQmlEngine(engine);
    }
}

void SurfaceContainer::addSurface(SurfaceWrapper *surface)
{
    doAddSurface(surface, true);
}

void SurfaceContainer::removeSurface(SurfaceWrapper *surface)
{
    doRemoveSurface(surface, true);
}

void SurfaceContainer::addOutput(Output *output)
{
    Q_ASSERT(output->isPrimary());
    const auto subContainers = this->subContainers();
    for (auto sub : subContainers) {
        sub->addOutput(output);
    }
}

void SurfaceContainer::removeOutput(Output *output)
{
    const auto subContainers = this->subContainers();
    for (auto sub : subContainers) {
        sub->removeOutput(output);
    }
}

void SurfaceContainer::geometryChange(const QRectF &newGeo, const QRectF &oldGeo)
{
    const auto subContainers = this->subContainers();
    for (SurfaceContainer *c : subContainers) {
        c->setPosition(newGeo.topLeft());
        c->setSize(newGeo.size());
    }

    QQuickItem::geometryChange(newGeo, oldGeo);
}

bool SurfaceContainer::doAddSurface(SurfaceWrapper *surface, bool setContainer)
{
    if (m_model->hasSurface(surface))
        return false;

    if (setContainer) {
        Q_ASSERT(!surface->container());
        surface->setContainer(this);
        surface->setParent(this);
    }

    m_model->addSurface(surface);
    Q_EMIT surfaceAdded(surface);

    if (auto p = parentContainer())
        p->addBySubContainer(this, surface);

    return true;
}

bool SurfaceContainer::doRemoveSurface(SurfaceWrapper *surface, bool setContainer)
{
    if (!m_model->hasSurface(surface))
        return false;

    if (setContainer) {
        Q_ASSERT(surface->container() == this);
        surface->setContainer(nullptr);
    }

    m_model->removeSurface(surface);
    Q_EMIT surfaceRemoved(surface);

    if (auto p = parentContainer())
        p->removeBySubContainer(this, surface);

    return true;
}

void SurfaceContainer::addBySubContainer([[maybe_unused]] SurfaceContainer *sub, SurfaceWrapper *surface)
{
    doAddSurface(surface, false);
}

void SurfaceContainer::removeBySubContainer([[maybe_unused]] SurfaceContainer *sub, SurfaceWrapper *surface)
{
    doRemoveSurface(surface, false);
}

bool SurfaceContainer::filterSurfaceGeometryChanged(SurfaceWrapper *surface, QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (auto p = parentContainer())
        return p->filterSurfaceGeometryChanged(surface, newGeometry, oldGeometry);
    return false;
}

bool SurfaceContainer::filterSurfaceStateChange(SurfaceWrapper *surface, SurfaceWrapper::State newState, SurfaceWrapper::State oldState)
{
    if (auto p = parentContainer())
        return p->filterSurfaceStateChange(surface, newState, oldState);
    return false;
}
