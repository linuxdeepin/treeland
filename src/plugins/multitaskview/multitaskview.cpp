// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "multitaskview.h"

#include "output/output.h"
#include "seat/helper.h"
#include "surface/surfacecontainer.h"
#include "workspace/workspace.h"
#include "treelandconfig.hpp"
#include "common/treelandlogging.h"

#include <woutputitem.h>
#include <woutputrenderwindow.h>

#include <QtConcurrentMap>

WAYLIB_SERVER_USE_NAMESPACE

Multitaskview::Multitaskview(QQuickItem *parent)
    : QQuickItem(parent)
    , m_status(Uninitialized)
    , m_activeReason(ShortcutKey)
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

qreal Multitaskview::partialFactor() const
{
    return m_partialFactor;
}

void Multitaskview::updatePartialFactor(qreal delta)
{
    qreal newPartialFactor = qBound(0.0, m_partialFactor + delta, 1.0);
    if (qFuzzyCompare(newPartialFactor, m_partialFactor))
        return;
    m_partialFactor = newPartialFactor;
    Q_EMIT partialFactorChanged();
}

void Multitaskview::exit(SurfaceWrapper *surface, bool immediately)
{
    Helper::instance()->setBlockActivateSurface(false);

    if (surface) {
        Helper::instance()->forceActivateSurface(surface);
    } else if (Helper::instance()->workspace()->current()->latestActiveSurface()) {
        // Should force activate latest active surface if there is.
        Helper::instance()->forceActivateSurface(
            Helper::instance()->workspace()->current()->latestActiveSurface());
    }

    Helper::instance()->setCurrentMode(Helper::CurrentMode::Normal);

    // TODO: handle taskview gesture
    Q_EMIT aboutToExit();

    if (immediately) {
        setVisible(false);
    } else {
        setStatus(Exited);
    }
}

void Multitaskview::enter(ActiveReason reason)
{
    Helper::instance()->activateSurface(nullptr);
    setActiveReason(reason);
    setStatus(Active);
    Helper::instance()->setCurrentMode(Helper::CurrentMode::Multitaskview);
}

MultitaskviewSurfaceModel::MultitaskviewSurfaceModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(Helper::instance()->workspace()->showOnAllWorkspaceModel(),
            &WorkspaceModel::surfaceRemoved,
            this,
            &MultitaskviewSurfaceModel::handleSurfaceRemoved);
}

void MultitaskviewSurfaceModel::initializeModel()
{
    if (!workspace() || !output() || m_layoutArea.isEmpty())
        return;
    beginResetModel();
    m_data.clear();
    QList<SurfaceWrapper *> surfaces(workspace()->surfaces());
    surfaces << Helper::instance()->workspace()->showOnAllWorkspaceModel()->surfaces();
    for (const auto &surface : std::as_const(surfaces)) {
        if (!Helper::instance()->surfaceBelongsToCurrentSession(surface))
            continue;
        if (surface->ownsOutput() == output()) {
            if (surfaceReady(surface)) {
                m_data.push_back(std::make_shared<SurfaceModelData>(
                    surface,
                    surfaceGeometry(surface)
                        .translated(-output()->geometry().topLeft())
                        .translated(-layoutArea().topLeft()),
                    false,
                    surface->isMinimized()));
            } else {
                monitorUnreadySurface(surface);
            }
        }
        connect(surface,
                &SurfaceWrapper::ownsOutputChanged,
                this,
                &MultitaskviewSurfaceModel::handleWrapperOutputChanged,
                Qt::UniqueConnection);
        connect(surface,
                &SurfaceWrapper::surfaceStateChanged,
                this,
                &MultitaskviewSurfaceModel::handleSurfaceStateChanged,
                Qt::UniqueConnection);
    }
    std::sort(m_data.begin(),
              m_data.end(),
              [this](const ModelDataPtr &lhs, const ModelDataPtr &rhs) -> bool {
                  return laterActiveThan(lhs->wrapper, rhs->wrapper);
              });
    doUpdateZOrder(m_data);
    endResetModel();
    m_modelReady = true;
    Q_EMIT countChanged();
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
    case MinimizedRole:
        return QVariant::fromValue(m_data[r]->minimized);
    case UpIndexRole:
        return QVariant::fromValue(m_data[r]->upIndex);
    case DownIndexRole:
        return QVariant::fromValue(m_data[r]->downIndex);
    case LeftIndexRole:
        return QVariant::fromValue(m_data[r]->leftIndex);
    case RightIndexRole:
        return QVariant::fromValue(m_data[r]->rightIndex);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MultitaskviewSurfaceModel::roleNames() const
{
    return QHash<int, QByteArray>{
        { SurfaceWrapperRole, "wrapper" }, { GeometryRole, "geometry" },
        { PaddingRole, "padding" },        { ZOrderRole, "zorder" },
        { MinimizedRole, "minimized" },    { UpIndexRole, "upIndex" },
        { DownIndexRole, "downIndex" },    { LeftIndexRole, "leftIndex" },
        { RightIndexRole, "rightIndex" }
    };
}

QModelIndex MultitaskviewSurfaceModel::index(int row, int column, [[maybe_unused]] const QModelIndex &parent) const
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
        Q_EMIT dataChanged(index(beginIndex),
                           index(endIndex),
                           { GeometryRole,
                             PaddingRole,
                             UpIndexRole,
                             DownIndexRole,
                             LeftIndexRole,
                             RightIndexRole });
    }
    Q_EMIT rowsChanged();
    Q_EMIT contentHeightChanged();
}

void MultitaskviewSurfaceModel::updateZOrder()
{
    doUpdateZOrder(m_data);
    Q_EMIT dataChanged(index(0), index(rowCount() - 1), { ZOrderRole });
}

uint MultitaskviewSurfaceModel::prevSameAppIndex(uint index)
{
    if (index >= count()) {
        qCWarning(treelandPlugin) << "prevSameAppIndex: invalid index" << index << "count:" << count();
        return index;
    }
    auto circularPrev = [this](uint i) {
        return (i + count() - 1) % count();
    };
    auto i = circularPrev(index);
    for (; i != index; i = circularPrev(i)) {
        if (m_data[i]->wrapper->appId()
            == m_data[index]->wrapper->appId()) {
            break;
        }
    }
    return i;
}

uint MultitaskviewSurfaceModel::nextSameAppIndex(uint index)
{
    if (index >= count()) {
        qCWarning(treelandPlugin) << "nextSameAppIndex: invalid index" << index << "count:" << count();
        return index;
    }
    auto circularNext = [this](uint i) {
        return (i + 1) % count();
    };
    auto i = circularNext(index);
    for (; i != index; i = circularNext(i)) {
        if (m_data[i]->wrapper->appId()
            == m_data[index]->wrapper->appId()) {
            break;
        }
    }
    return i;
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
    Q_EMIT layoutAreaChanged();
}

bool MultitaskviewSurfaceModel::tryLayout(const QList<ModelDataPtr> &rawData,
                                          qreal rowH,
                                          bool ignoreOverlap)
{
    int nrows = 1;
    qreal acc = 0;
    auto devicePixelRatio = output()->outputItem()->devicePixelRatio();
    auto topContentMargin =
        Helper::instance()->config()->multitaskviewTopContentMargin() / devicePixelRatio;
    auto bottomContentMargin =
        Helper::instance()->config()->multitaskviewBottomContentMargin() / devicePixelRatio;
    auto cellPadding = Helper::instance()->config()->multitaskviewCellPadding() / devicePixelRatio;
    auto horizontalMargin =
        Helper::instance()->config()->multitaskviewHorizontalMargin() / devicePixelRatio;
    auto availWidth = std::max(0.0, layoutArea().width() - 2 * horizontalMargin);
    auto availHeight =
        std::max(0.0, layoutArea().height() - topContentMargin - bottomContentMargin);
    if (availWidth <= 0)
        return false;
    QList<QList<ModelDataPtr>> rowstmp;
    QList<ModelDataPtr> currow;
    for (auto &modelData : rawData) {
        auto surface = modelData->wrapper;
        auto whRatio = surface->width() / surface->height();
        modelData->pendingPadding = surface->height() < (rowH - 2 * cellPadding);
        auto curW = std::min(availWidth,
                             whRatio * std::min(rowH - 2 * cellPadding, surface->height())
                                 + 2 * cellPadding);
        modelData->pendingGeometry.setWidth(curW - 2 * cellPadding);
        auto newAcc = acc + curW;
        if (newAcc <= availWidth) {
            acc = newAcc;
            currow.append(modelData);
        } else if (newAcc / availWidth > Helper::instance()->config()->multitaskviewLoadFactor()) {
            acc = curW;
            nrows++;
            rowstmp.append(currow);
            currow = { modelData };
        } else {
            // Just scale the last element
            curW = availWidth - acc;
            modelData->pendingGeometry.setWidth(curW - 2 * cellPadding);
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

void MultitaskviewSurfaceModel::calcDisplayPos(const QList<ModelDataPtr> &rawData)
{
    auto devicePixelRatio = output()->outputItem()->devicePixelRatio();
    auto topContentMargin =
        Helper::instance()->config()->multitaskviewTopContentMargin() / devicePixelRatio;
    auto bottomContentMargin =
        Helper::instance()->config()->multitaskviewBottomContentMargin() / devicePixelRatio;
    auto cellPadding = Helper::instance()->config()->multitaskviewCellPadding() / devicePixelRatio;
    auto horizontalMargin =
        Helper::instance()->config()->multitaskviewHorizontalMargin() / devicePixelRatio;
    auto availWidth = std::max(0.0, layoutArea().width() - 2 * horizontalMargin);
    auto availHeight =
        std::max(0.0, layoutArea().height() - topContentMargin - bottomContentMargin);
    auto contentHeight = m_rows.length() * m_rowHeight;
    auto curY = std::max(availHeight - contentHeight, 0.0) / 2 + topContentMargin;
    const auto hCenter = availWidth / 2;
    for (auto i = 0; i < m_rows.size(); ++i) {
        auto row = m_rows[i];
        const auto totW = QtConcurrent::blockingMappedReduced(
            row,
            [cellPadding](ModelDataPtr data) {
                return data->pendingGeometry.width() + 2 * cellPadding;
            },
            [](qreal &acc, const qreal &cur) {
                acc += cur;
            });
        auto curX = hCenter - totW / 2 + cellPadding + horizontalMargin;
        for (auto j = 0; j < row.size(); ++j) {
            auto window = row[j];
            window->pendingGeometry.moveLeft(curX);
            window->pendingGeometry.moveTop(curY);
            window->pendingGeometry.setHeight(m_rowHeight - 2 * cellPadding);
            window->pendingLeftIndex = rawData.indexOf(row[(j - 1 + row.size()) % row.size()]);
            window->pendingRightIndex = rawData.indexOf(row[(j + 1) % row.size()]);
            auto lastRow = m_rows[std::max(0, i - 1)];
            auto lastRowIndex = std::min(static_cast<int>(lastRow.size()) - 1, j);
            window->pendingUpIndex = rawData.indexOf(lastRow[lastRowIndex]);
            auto nextRow = m_rows[std::min(static_cast<int>(m_rows.size()) - 1, i + 1)];
            auto nextRowIndex = std::min(static_cast<int>(nextRow.size()) - 1, j);
            window->pendingDownIndex = rawData.indexOf(nextRow[nextRowIndex]);
            curX += window->pendingGeometry.width() + 2 * cellPadding;
        }
        curY += m_rowHeight;
    }
    m_contentHeight = curY;
}

void MultitaskviewSurfaceModel::doCalculateLayout(const QList<ModelDataPtr> &rawData)
{
    auto devicePixelRatio = output()->outputItem()->devicePixelRatio();
    auto maxWindowHeight =
        std::min(layoutArea().height(),
                 static_cast<qreal>(Helper::instance()->config()->normalWindowHeight() / devicePixelRatio));
    auto minWindowHeight = Helper::instance()->config()->minMultitaskviewSurfaceHeight() / devicePixelRatio;
    auto windowHeightStep = Helper::instance()->config()->windowHeightStep() / devicePixelRatio;
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
    calcDisplayPos(rawData);
}

void MultitaskviewSurfaceModel::doUpdateZOrder(const QList<ModelDataPtr> &rawData)
{
    auto surfaces = WOutputRenderWindow::paintOrderItemList(
        Helper::instance()->workspace(),
        [this](QQuickItem *item) -> bool {
            auto surfaceWrapper = qobject_cast<SurfaceWrapper *>(item);
            if (surfaceWrapper && surfaceWrapper->showOnWorkspace(workspace()->id())) {
                return true;
            } else {
                return false;
            }
        });
    std::for_each(rawData.begin(), rawData.end(), [surfaces](ModelDataPtr modelData) {
        modelData->zorder = surfaces.indexOf(modelData->wrapper);
    });
}

std::pair<int, int> MultitaskviewSurfaceModel::commitAndGetUpdateRange(
    [[maybe_unused]] const QList<ModelDataPtr> &rawData)
{
    // TODO: better algorithm
    int beginIndex = 0, endIndex = -1;
    bool unchanged = true;
    for (int i = 0; i < m_data.size(); ++i) {
        if (m_data[i]->padding != m_data[i]->pendingPadding
            || m_data[i]->geometry != m_data[i]->pendingGeometry
            || m_data[i]->upIndex != m_data[i]->pendingUpIndex
            || m_data[i]->downIndex != m_data[i]->pendingDownIndex
            || m_data[i]->leftIndex != m_data[i]->pendingLeftIndex
            || m_data[i]->rightIndex != m_data[i]->pendingRightIndex) {
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
    Q_ASSERT(wrapper);
    if (surfaceReady(wrapper)) {
        addReadySurface(wrapper);
    }
}

void MultitaskviewSurfaceModel::handleWrapperOutputChanged()
{
    auto wrapper = qobject_cast<SurfaceWrapper *>(sender());
    Q_ASSERT(wrapper);
    if (wrapper->ownsOutput() == output()) {
        if (surfaceReady(wrapper)) {
            addReadySurface(wrapper);
        } else {
            monitorUnreadySurface(wrapper);
        }
    }
}

void MultitaskviewSurfaceModel::handleSurfaceStateChanged()
{
    auto surface = qobject_cast<SurfaceWrapper *>(sender());
    Q_ASSERT(surface);
    auto dataPtr =
        std::find_if(m_data.begin(), m_data.end(), [surface](const ModelDataPtr &modelData) {
            return modelData->wrapper == surface;
        });
    if (dataPtr == m_data.end())
        return;
    if (surface->isMinimized() != (*dataPtr)->minimized) {
        (*dataPtr)->minimized = surface->isMinimized();
        int i = std::distance(m_data.begin(), dataPtr);
        Q_EMIT dataChanged(index(i), index(i), { MinimizedRole });
    }
}

void MultitaskviewSurfaceModel::handleSurfaceMappedChanged()
{
    auto surface = qobject_cast<WSurface *>(sender());
    auto it = std::find_if(workspace()->surfaces().begin(),
                           workspace()->surfaces().end(),
                           [surface](SurfaceWrapper *wrapper) {
                               return wrapper->surface() == surface;
                           });
    Q_ASSERT_X(it != workspace()->surfaces().end(),
               __func__,
               "Monitoring mapped of a removed surface wrapper.");
    if (surfaceReady(*it)) {
        addReadySurface(*it);
    }
}

void MultitaskviewSurfaceModel::handleSurfaceAdded(SurfaceWrapper *surface)
{
    if (!Helper::instance()->surfaceBelongsToCurrentSession(surface))
        return;
    connect(surface,
            &SurfaceWrapper::ownsOutputChanged,
            this,
            &MultitaskviewSurfaceModel::handleWrapperOutputChanged,
            Qt::UniqueConnection);
    connect(surface,
            &SurfaceWrapper::surfaceStateChanged,
            this,
            &MultitaskviewSurfaceModel::handleSurfaceStateChanged,
            Qt::UniqueConnection);
    if (surface->ownsOutput() == output()) {
        if (surfaceReady(surface)) {
            addReadySurface(surface);
        } else {
            monitorUnreadySurface(surface);
        }
    }
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
    disconnect(surface,
               &SurfaceWrapper::ownsOutputChanged,
               this,
               &MultitaskviewSurfaceModel::handleWrapperOutputChanged);
    disconnect(surface,
               &SurfaceWrapper::surfaceStateChanged,
               this,
               &MultitaskviewSurfaceModel::handleSurfaceStateChanged);
    endRemoveRows();
    doCalculateLayout(m_data);
    auto [beginIndex, endIndex] = commitAndGetUpdateRange(m_data);
    if (beginIndex <= endIndex) {
        Q_ASSERT(beginIndex < m_data.size());
        Q_EMIT dataChanged(index(beginIndex),
                           index(endIndex),
                           { GeometryRole,
                             PaddingRole,
                             UpIndexRole,
                             DownIndexRole,
                             LeftIndexRole,
                             RightIndexRole });
    }
    Q_EMIT rowsChanged();
    Q_EMIT countChanged();
    Q_EMIT contentHeightChanged();
}

void MultitaskviewSurfaceModel::addReadySurface(SurfaceWrapper *surface)
{
    Q_ASSERT_X(surfaceReady(surface),
               __func__,
               "Surface wrapper should be ready before adding to multitaskview model.");
    disconnect(surface,
               &SurfaceWrapper::normalGeometryChanged,
               this,
               &MultitaskviewSurfaceModel::handleWrapperGeometryChanged);
    disconnect(surface,
               &SurfaceWrapper::geometryChanged,
               this,
               &MultitaskviewSurfaceModel::handleWrapperGeometryChanged);
    disconnect(surface->surface(),
               &WSurface::mappedChanged,
               this,
               &MultitaskviewSurfaceModel::handleSurfaceMappedChanged);
    auto toBeInserted =
        std::make_shared<SurfaceModelData>(surface,
                                           surfaceGeometry(surface)
                                               .translated(-output()->geometry().topLeft())
                                               .translated(-layoutArea().topLeft()),
                                           false,
                                           surface->isMinimized());
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
        Q_EMIT dataChanged(index(beginIndex),
                           index(endIndex),
                           { GeometryRole,
                             PaddingRole,
                             UpIndexRole,
                             DownIndexRole,
                             LeftIndexRole,
                             RightIndexRole });
    }
    toBeInserted->commit();
    beginInsertRows({}, insertedIndex, insertedIndex);
    m_data = pendingData;
    pendingData.clear();
    endInsertRows();
    Q_EMIT rowsChanged();
    Q_EMIT countChanged();
    Q_EMIT contentHeightChanged();
}

void MultitaskviewSurfaceModel::monitorUnreadySurface(SurfaceWrapper *surface)
{
    Q_ASSERT_X(!surfaceReady(surface), __func__, "Surface is ready.");
    connect(surface,
            &SurfaceWrapper::normalGeometryChanged,
            this,
            &MultitaskviewSurfaceModel::handleWrapperGeometryChanged,
            Qt::UniqueConnection);
    connect(surface,
            &SurfaceWrapper::geometryChanged,
            this,
            &MultitaskviewSurfaceModel::handleWrapperGeometryChanged,
            Qt::UniqueConnection);
    connect(surface->surface(),
            &WSurface::mappedChanged,
            this,
            &MultitaskviewSurfaceModel::handleSurfaceMappedChanged,
            Qt::UniqueConnection);
}

bool MultitaskviewSurfaceModel::surfaceReady(SurfaceWrapper *surface)
{
    return surface->surface()->mapped() && surfaceGeometry(surface).isValid();
}

QRectF MultitaskviewSurfaceModel::surfaceGeometry(SurfaceWrapper *surface)
{
    if (surface->isMinimized()) {
        return surface->normalGeometry();
    } else {
        return surface->geometry();
    }
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
    Q_EMIT workspaceChanged();
}

qreal MultitaskviewSurfaceModel::contentHeight() const
{
    return m_contentHeight;
}

Output *MultitaskviewSurfaceModel::output() const
{
    return m_output;
}

void MultitaskviewSurfaceModel::setOutput(Output *newOutput)
{
    if (m_output == newOutput)
        return;
    m_output = newOutput;
    initializeModel();
    Q_EMIT outputChanged();
}

uint MultitaskviewSurfaceModel::count() const
{
    return rowCount();
}
