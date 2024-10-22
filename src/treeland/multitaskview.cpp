// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "multitaskview.h"

#include "helper.h"
#include "surfacecontainer.h"
#include "treelandconfig.h"
#include "workspace.h"

#include <woutputrenderwindow.h>

#include <QtConcurrentMap>

WAYLIB_SERVER_USE_NAMESPACE

Multitaskview::Multitaskview(QQuickItem *parent)
    : QQuickItem(parent)
    , m_status(Uninitialized)
    , m_activeReason(ShortcutKey)
    , m_taskviewVal(0.0)
{
}

Multitaskview::Status Multitaskview::status() const
{
    return m_status;
}

void Multitaskview::setStatus(Status status)
{
    if (status == m_status)
        return;
    m_status = status;
    Q_EMIT statusChanged();
}

Multitaskview::ActiveReason Multitaskview::activeReason() const
{
    return m_activeReason;
}

void Multitaskview::setActiveReason(ActiveReason activeReason)
{
    if (activeReason == m_activeReason)
        return;
    m_activeReason = activeReason;
    Q_EMIT activeReasonChanged();
}

qreal Multitaskview::taskviewVal() const
{
    return m_taskviewVal;
}

void Multitaskview::setTaskviewVal(qreal taskviewVal)
{
    if (taskviewVal == m_taskviewVal)
        return;
    m_taskviewVal = taskviewVal;
    Q_EMIT taskviewValChanged();
}

void Multitaskview::exit(SurfaceWrapper *surface)
{
    if (surface) {
        Helper::instance()->forceActivateSurface(surface);
    }
    // TODO: handle taskview gesture
    setStatus(Exited);
}

void Multitaskview::enter(ActiveReason reason)
{
    setStatus(Active);
    setActiveReason(reason);
}

MultitaskviewSurfaceModel::MultitaskviewSurfaceModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void MultitaskviewSurfaceModel::initializeModel()
{
    if (!workspace() || m_layoutArea.isEmpty())
        return;
    beginResetModel();
    m_data.clear();
    std::transform(workspace()->surfaces().begin(),
                   workspace()->surfaces().end(),
                   std::back_inserter(m_data),
                   [this](SurfaceWrapper *wrapper) -> ModelDataPtr {
                       return std::make_shared<SurfaceModelData>(
                           wrapper,
                           wrapper->geometry().translated(-layoutArea().topLeft()),
                           false);
                   });
    doUpdateZOrder(m_data);
    endResetModel();
    m_modelReady = true;
    Q_EMIT modelReadyChanged();
}

int MultitaskviewSurfaceModel::rowCount([[maybe_unused]] const QModelIndex &parent) const
{
    return m_data.count();
}

QVariant MultitaskviewSurfaceModel::data(const QModelIndex &index, int role) const
{
    auto r = index.row();
    if (r < 0 || r >= rowCount(index.parent()))
        return QVariant();
    switch (role) {
    case SurfaceWrapperRole:
        return QVariant::fromValue(m_data[r]->wrapper);
    case GeometryRole:
        return QVariant::fromValue(m_data[r]->geometry);
    case PaddingRole:
        return QVariant::fromValue(m_data[r]->padding);
    case ZOrderRole:
        return QVariant::fromValue(m_data[r]->zorder);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MultitaskviewSurfaceModel::roleNames() const
{
    return QHash<int, QByteArray>{ { SurfaceWrapperRole, "wrapper" },
                                   { GeometryRole, "geometry" },
                                   { PaddingRole, "padding" },
                                   { ZOrderRole, "zorder" } };
}

QModelIndex MultitaskviewSurfaceModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || row >= m_data.size())
        return QModelIndex();
    return QAbstractItemModel::createIndex(row, column, &m_data[row]);
}

bool MultitaskviewSurfaceModel::modelReady() const
{
    return m_modelReady;
}

void MultitaskviewSurfaceModel::calcLayout()
{
    doCalculateLayout(m_data);
    auto [beginIndex, endIndex] = commitAndGetUpdateRange(m_data);
    if (beginIndex <= endIndex) {
        Q_EMIT dataChanged(index(beginIndex), index(endIndex), { GeometryRole, PaddingRole });
    }
    Q_EMIT rowsChanged();
}

void MultitaskviewSurfaceModel::updateZOrder()
{
    doUpdateZOrder(m_data);
    Q_EMIT dataChanged(index(0), index(rowCount() - 1), { ZOrderRole });
}

QRectF MultitaskviewSurfaceModel::layoutArea() const
{
    return m_layoutArea;
}

void MultitaskviewSurfaceModel::setLayoutArea(const QRectF &newLayoutArea)
{
    if (m_layoutArea == newLayoutArea)
        return;
    m_layoutArea = newLayoutArea;
    initializeModel();
    emit layoutAreaChanged();
}

bool MultitaskviewSurfaceModel::tryLayout(const QList<ModelDataPtr> &rawData,
                                          qreal rowH,
                                          bool ignoreOverlap)
{
    int nrows = 1;
    qreal acc = 0;
    auto availWidth = layoutArea().width();
    auto availHeight = layoutArea().height();
    if (availWidth <= 0)
        return false;
    QList<QList<ModelDataPtr>> rowstmp;
    QList<ModelDataPtr> currow;
    for (auto &modelData : rawData) {
        auto surface = modelData->wrapper;
        auto whRatio = surface->width() / surface->height();
        modelData->pendingPadding = surface->height() < (rowH - 2 * CellPadding);
        auto curW = std::min(availWidth,
                             whRatio * std::min(rowH - 2 * CellPadding, surface->height())
                                 + 2 * CellPadding);
        modelData->pendingGeometry.setWidth(curW - 2 * CellPadding);
        auto newAcc = acc + curW;
        if (newAcc <= availWidth) {
            acc = newAcc;
            currow.append(modelData);
        } else if (newAcc / availWidth > LoadFactor) {
            acc = curW;
            nrows++;
            rowstmp.append(currow);
            currow = { modelData };
        } else {
            // Just scale the last element
            curW = availWidth - acc;
            modelData->pendingGeometry.setWidth(curW - 2 * CellPadding);
            currow.append(modelData);
            acc = newAcc;
        }
    }
    if (nrows * rowH <= availHeight || ignoreOverlap) {
        if (currow.length()) {
            rowstmp.append(currow);
        }
        m_rowHeight = rowH;
        m_rows = rowstmp;
        return true;
    }
    return false;
}

void MultitaskviewSurfaceModel::calcDisplayPos()
{
    auto availHeight = layoutArea().height();
    auto availWidth = layoutArea().width();
    auto contentHeight = m_rows.length() * m_rowHeight;
    auto curY = std::max(availHeight - contentHeight, 0.0) / 2 + TopContentMargin;
    const auto hCenter = availWidth / 2;
    for (const auto &row : std::as_const(m_rows)) {
        const auto totW = QtConcurrent::blockingMappedReduced(
            row,
            [](ModelDataPtr data) {
                return data->pendingGeometry.width() + 2 * CellPadding;
            },
            [](qreal &acc, const qreal &cur) {
                acc += cur;
            });
        auto curX = hCenter - totW / 2 + CellPadding;
        for (auto &window : row) {
            window->pendingGeometry.moveLeft(curX);
            window->pendingGeometry.moveTop(curY);
            window->pendingGeometry.setHeight(m_rowHeight - 2 * CellPadding);
            curX += window->pendingGeometry.width() + 2 * CellPadding;
        }
        curY += m_rowHeight;
    }
    m_contentHeight = curY;
}

void MultitaskviewSurfaceModel::doCalculateLayout(const QList<ModelDataPtr> &rawData)
{
    auto maxWindowHeight = std::min(layoutArea().height(),
                                    static_cast<qreal>(TreelandConfig::ref().normalWindowHeight()));
    auto minWindowHeight = TreelandConfig::ref().minMultitaskviewSurfaceHeight();
    auto windowHeightStep = TreelandConfig::ref().windowHeightStep();
    auto rowH = maxWindowHeight;
    while (rowH > minWindowHeight) {
        if (tryLayout(rawData, rowH)) {
            break;
        }
        rowH -= windowHeightStep;
    }
    if (rowH < minWindowHeight) {
        tryLayout(rawData, minWindowHeight, true);
    }
    calcDisplayPos();
}

void MultitaskviewSurfaceModel::doUpdateZOrder(const QList<ModelDataPtr> &rawData)
{
    auto surfaces = WOutputRenderWindow::paintOrderItemList(
        Helper::instance()->workspace(),
        [this](QQuickItem *item) -> bool {
            auto surfaceWrapper = qobject_cast<SurfaceWrapper *>(item);
            if (surfaceWrapper
                && (surfaceWrapper->showOnWorkspace(workspace()->id())
                    || surfaceWrapper->showOnAllWorkspace())) {
                return true;
            } else {
                return false;
            }
        });
    std::for_each(rawData.begin(), rawData.end(), [surfaces, this](ModelDataPtr modelData) {
        modelData->zorder = surfaces.indexOf(modelData->wrapper);
    });
}

std::pair<int, int> MultitaskviewSurfaceModel::commitAndGetUpdateRange(
    const QList<ModelDataPtr> &rawData)
{
    // TODO: better algorithm
    int beginIndex = 0, endIndex = -1;
    bool unchanged = true;
    for (int i = 0; i < m_data.size(); ++i) {
        if (m_data[i]->padding != m_data[i]->pendingPadding
            || m_data[i]->geometry != m_data[i]->pendingGeometry) {
            endIndex = i;
            unchanged = false;
        } else {
            if (unchanged) {
                beginIndex = i + 1;
            }
        }
        m_data[i]->commit();
    }
    return { beginIndex, endIndex };
}

void MultitaskviewSurfaceModel::handleWrapperGeometryChanged()
{
    auto wrapper = qobject_cast<SurfaceWrapper *>(sender());
    qDebug() << "wrapper" << wrapper << "geometry change to" << wrapper->geometry();
    if (wrapper->geometry().isValid() && wrapper->surface()->mapped()) {
        addReadySurface(wrapper);
    }
}

void MultitaskviewSurfaceModel::handleSurfaceMappedChanged()
{
    auto surface = qobject_cast<WSurface *>(sender());
    qDebug() << "surface" << surface << "mapped" << surface->mapped();
    auto it = std::find_if(workspace()->surfaces().begin(),
                           workspace()->surfaces().end(),
                           [surface](SurfaceWrapper *wrapper) {
                               return wrapper->surface() == surface;
                           });
    Q_ASSERT_X(it != workspace()->surfaces().end(),
               __func__,
               "Monitoring mapped of a removed surface wrapper.");
    if ((*it)->geometry().isValid() && surface->mapped()) {
        addReadySurface(*it);
    }
}

void MultitaskviewSurfaceModel::handleSurfaceAdded(SurfaceWrapper *surface)
{
    if (!surface->surface()->mapped() || !surface->geometry().isValid()) {
        connect(surface,
                &SurfaceWrapper::geometryChanged,
                this,
                &MultitaskviewSurfaceModel::handleWrapperGeometryChanged);
        connect(surface->surface(),
                &WSurface::mappedChanged,
                this,
                &MultitaskviewSurfaceModel::handleSurfaceMappedChanged);
        return;
    }
    addReadySurface(surface);
}

void MultitaskviewSurfaceModel::handleSurfaceRemoved(SurfaceWrapper *surface)
{
    auto toBeRemovedIt =
        std::find_if(m_data.begin(), m_data.end(), [surface](ModelDataPtr modelData) {
            return modelData->wrapper == surface;
        });
    if (toBeRemovedIt == m_data.end())
        return;
    int toRemove = std::distance(m_data.begin(), toBeRemovedIt);
    beginRemoveRows({}, toRemove, toRemove);
    m_data.remove(toRemove);
    endRemoveRows();
    doCalculateLayout(m_data);
    auto [beginIndex, endIndex] = commitAndGetUpdateRange(m_data);
    if (beginIndex <= endIndex) {
        Q_ASSERT(beginIndex < m_data.size());
        Q_EMIT dataChanged(index(beginIndex), index(endIndex), { GeometryRole, PaddingRole });
    }
}

void MultitaskviewSurfaceModel::addReadySurface(SurfaceWrapper *surface)
{
    disconnect(surface,
               &SurfaceWrapper::geometryChanged,
               this,
               &MultitaskviewSurfaceModel::handleWrapperGeometryChanged);
    disconnect(surface->surface(),
               &WSurface::mappedChanged,
               this,
               &MultitaskviewSurfaceModel::handleSurfaceMappedChanged);
    Q_ASSERT_X(surface->surface()->mapped() && surface->geometry().isValid(),
               __func__,
               "Surface wrapper should be ready before adding to multitaskview model.");
    auto toBeInserted =
        std::make_shared<SurfaceModelData>(surface,
                                           surface->geometry().translated(-layoutArea().topLeft()),
                                           false);
    QList<ModelDataPtr> pendingData{ m_data };
    auto it = pendingData.begin();
    for (; it != pendingData.end() && laterActiveThan((*it)->wrapper, surface); ++it)
        ;
    auto insertedIt = pendingData.insert(it, toBeInserted);
    int insertedIndex = std::distance(pendingData.begin(), insertedIt);
    Q_ASSERT(insertedIndex >= 0 && insertedIndex < pendingData.size());
    doCalculateLayout(pendingData);
    auto [beginIndex, endIndex] = commitAndGetUpdateRange(m_data);
    if (beginIndex <= endIndex) {
        Q_ASSERT(beginIndex < m_data.size());
        Q_EMIT dataChanged(index(beginIndex), index(endIndex), { GeometryRole, PaddingRole });
    }
    toBeInserted->commit();
    beginInsertRows({}, insertedIndex, insertedIndex);
    m_data = pendingData;
    pendingData.clear();
    endInsertRows();
}

bool MultitaskviewSurfaceModel::laterActiveThan(SurfaceWrapper *a, SurfaceWrapper *b)
{
    auto activeIndex = [this](SurfaceWrapper *surface) {
        auto it = std::find(workspace()->m_activedSurfaceHistory.begin(),
                            workspace()->m_activedSurfaceHistory.end(),
                            surface);
        return std::distance(workspace()->m_activedSurfaceHistory.begin(), it);
    };
    return activeIndex(a) < activeIndex(b);
}

void MultitaskviewSurfaceModel::connectWorkspace(WorkspaceModel *workspace)
{
    connect(workspace,
            &WorkspaceModel::surfaceAdded,
            this,
            &MultitaskviewSurfaceModel::handleSurfaceAdded);
    connect(workspace,
            &WorkspaceModel::surfaceRemoved,
            this,
            &MultitaskviewSurfaceModel::handleSurfaceRemoved);
}

void MultitaskviewSurfaceModel::disconnectWorkspace(WorkspaceModel *workspace)
{
    disconnect(workspace,
               &WorkspaceModel::surfaceAdded,
               this,
               &MultitaskviewSurfaceModel::handleSurfaceAdded);
    disconnect(workspace,
               &WorkspaceModel::surfaceRemoved,
               this,
               &MultitaskviewSurfaceModel::handleSurfaceRemoved);
}

uint MultitaskviewSurfaceModel::rows() const
{
    return m_rows.count();
}

WorkspaceModel *MultitaskviewSurfaceModel::workspace() const
{
    return m_workspace;
}

void MultitaskviewSurfaceModel::setWorkspace(WorkspaceModel *newWorkspace)
{
    if (m_workspace == newWorkspace)
        return;
    if (m_workspace)
        disconnectWorkspace(m_workspace);
    m_workspace = newWorkspace;
    if (m_workspace)
        connectWorkspace(m_workspace);
    initializeModel();
    emit workspaceChanged();
}
