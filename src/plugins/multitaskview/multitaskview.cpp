// Copyright (C) 2024-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "multitaskview.h"

#include "common/treelandlogging.h"
#include "output/output.h"
#include "core/waylandserver.h"
#include "surface/surfacecontainer.h"
#include "treelanduserconfig.hpp"
#include "workspace/workspace.h"

#include <woutputitem.h>
#include <woutputrenderwindow.h>

#include <QtConcurrentMap>
#include <QQuickWindow>

#include <ranges>

WAYLIB_SERVER_USE_NAMESPACE

Multitaskview::Multitaskview(QQuickItem *parent)
    : QQuickItem(parent)
    , m_status(Uninitialized)
    , m_activeReason(ShortcutKey)
    , m_initialWorkspaceId(WaylandServer::instance()->workspace()->current()->id())
{
}

Multitaskview::Status Multitaskview::status() const
{
    return m_status;
}

void Multitaskview::setStatus(Status status)
{
    if (status != m_status)
        m_status = status;
    Q_EMIT actionFinished();
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

void Multitaskview::updatePartialFactor(qreal progress)
{
    qreal newPartialFactor = qBound(0.0, progress, 1.0);
    if (qFuzzyCompare(newPartialFactor, m_partialFactor))
        return;
    m_partialFactor = newPartialFactor;
    Q_EMIT partialFactorChanged();
}

void Multitaskview::exit(SurfaceWrapper *surface, bool immediately, bool restoreFocus)
{
    // Stop tracking keyboard focus before handing it back to a client surface,
    // otherwise forceActivateSurface() would re-enter exit() recursively.
    if (auto *w = window()) {
        disconnect(w,
                   &QQuickWindow::activeFocusItemChanged,
                   this,
                   &Multitaskview::onActiveFocusItemChanged);
    }

    WaylandServer::instance()->setBlockActivateSurface(false);

    // Browsing other workspaces commits a switch only when leaving the overview.
    if (WaylandServer::instance()->workspace()->current()->id() != m_initialWorkspaceId)
        WaylandServer::instance()->cancelShowDesktop();

    // Dismissing the view without selecting a window must keep showing the desktop.
    if (restoreFocus
        && (surface || WaylandServer::instance()->showDesktopState()
                           != ShowDesktopInterfaceV1::State::Show)) {
        if (!surface)
            surface = WaylandServer::instance()->workspace()->current()->latestActiveSurface();
        if (surface)
            WaylandServer::instance()->forceActivateSurface(surface);
    }

    if (WaylandServer::instance()->currentMode() == WaylandServer::CurrentMode::Multitaskview) {
        WaylandServer::instance()->setCurrentMode(WaylandServer::CurrentMode::Normal);
    }
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
    // Keep the hidden windows and their activation state for the next desktop toggle.
    if (WaylandServer::instance()->showDesktopState() == ShowDesktopInterfaceV1::State::Show)
        WaylandServer::instance()->requestKeyboardFocus(nullptr);
    else
        WaylandServer::instance()->activateSurface(nullptr);
    if (reason == ActiveReason::ShortcutKey)
        WaylandServer::instance()->setCurrentMode(WaylandServer::CurrentMode::Multitaskview);
    setActiveReason(reason);
    setStatus(Active);

    // Auto-exit when keyboard focus moves outside the view, including when
    // a client surface is activated.
    if (auto *w = window()) {
        connect(w,
                &QQuickWindow::activeFocusItemChanged,
                this,
                &Multitaskview::onActiveFocusItemChanged,
                Qt::UniqueConnection);
    }
}

void Multitaskview::onActiveFocusItemChanged()
{
    if (m_status != Active || !isVisible())
        return;

    auto *w = window();
    auto *item = w ? w->activeFocusItem() : nullptr;
    // Ignore transient null focus and focus changes within the view.
    if (!item || item == this || isAncestorOf(item))
        return;

    qCDebug(lcTlPlugin) << "Multitask view lost keyboard focus to" << item
                        << "- exiting the multitask view";
    exit(nullptr, /*immediately=*/false, /*restoreFocus=*/false);
}

void Multitaskview::commitGesture(bool triggered)
{
    if (m_gestureCommitted == triggered)
        return;
    m_gestureCommitted = triggered;
    if (triggered) {
        WaylandServer::instance()->setCurrentMode(WaylandServer::CurrentMode::Multitaskview);
    } else if (WaylandServer::instance()->currentMode() == WaylandServer::CurrentMode::Multitaskview) {
        WaylandServer::instance()->setCurrentMode(WaylandServer::CurrentMode::Normal);
    }
}
MultitaskviewSurfaceModel::MultitaskviewSurfaceModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(WaylandServer::instance()->workspace()->showOnAllWorkspaceModel(),
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
    surfaces << WaylandServer::instance()->workspace()->showOnAllWorkspaceModel()->surfaces();
    for (const auto &surface : std::as_const(surfaces)) {
        if (!WaylandServer::instance()->surfaceBelongsToCurrentSession(surface))
            continue;
        if (surface->skipMutiTaskView())
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
                if (auto wsurface = surface->surface()) {
                    connect(wsurface,
                            &WSurface::mappedChanged,
                            this,
                            &MultitaskviewSurfaceModel::handleSurfaceMappedChanged,
                            Qt::UniqueConnection);
                }
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
                &SurfaceWrapper::minimizedChanged,
                this,
                &MultitaskviewSurfaceModel::handleMinimizedChanged,
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
        qCWarning(lcTlPlugin) << "prevSameAppIndex: invalid index" << index << "count:" << count();
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
        qCWarning(lcTlPlugin) << "nextSameAppIndex: invalid index" << index << "count:" << count();
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
        WaylandServer::instance()->config()->multitaskviewTopContentMargin() / devicePixelRatio;
    auto bottomContentMargin =
        WaylandServer::instance()->config()->multitaskviewBottomContentMargin() / devicePixelRatio;
    auto cellPadding = WaylandServer::instance()->config()->multitaskviewCellPadding() / devicePixelRatio;
    auto horizontalMargin =
        WaylandServer::instance()->config()->multitaskviewHorizontalMargin() / devicePixelRatio;
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
        } else if (newAcc / availWidth > WaylandServer::instance()->config()->multitaskviewLoadFactor()) {
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
        WaylandServer::instance()->config()->multitaskviewTopContentMargin() / devicePixelRatio;
    auto bottomContentMargin =
        WaylandServer::instance()->config()->multitaskviewBottomContentMargin() / devicePixelRatio;
    auto cellPadding = WaylandServer::instance()->config()->multitaskviewCellPadding() / devicePixelRatio;
    auto horizontalMargin =
        WaylandServer::instance()->config()->multitaskviewHorizontalMargin() / devicePixelRatio;
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
                 static_cast<qreal>(WaylandServer::instance()->config()->normalWindowHeight() / devicePixelRatio));
    auto minWindowHeight = WaylandServer::instance()->config()->minMultitaskviewSurfaceHeight() / devicePixelRatio;
    auto windowHeightStep = WaylandServer::instance()->config()->windowHeightStep() / devicePixelRatio;
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
        WaylandServer::instance()->workspace(),
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

void MultitaskviewSurfaceModel::handleMinimizedChanged()
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
    if (!surface) {
        qCWarning(lcTlPlugin) << "handleSurfaceMappedChanged: sender is not a WSurface";
        return;
    }

    auto findWrapper = [surface](const QList<SurfaceWrapper *> &surfaces) -> SurfaceWrapper * {
        auto it = std::ranges::find(surfaces, surface, [](SurfaceWrapper *wrapper) {
            return wrapper->surface();
        });
        return it != surfaces.end() ? *it : nullptr;
    };

    SurfaceWrapper *wrapper = findWrapper(workspace()->surfaces());
    if (!wrapper) {
        wrapper =
            findWrapper(WaylandServer::instance()->workspace()->showOnAllWorkspaceModel()->surfaces());
    }
    Q_ASSERT_X(wrapper, __func__, "Monitoring mapped of a removed surface wrapper.");

    if (surface->mapped()) {
        if (surfaceReady(wrapper)) {
            addReadySurface(wrapper);
        }
    } else { // unmapped
        removeReadySurface(wrapper);
        if (wrapper->ownsOutput() == output()) {
            monitorUnreadySurface(wrapper);
        }
    }
}

void MultitaskviewSurfaceModel::handleSurfaceAdded(SurfaceWrapper *surface)
{
    if (!WaylandServer::instance()->surfaceBelongsToCurrentSession(surface))
        return;
    connect(surface,
            &SurfaceWrapper::ownsOutputChanged,
            this,
            &MultitaskviewSurfaceModel::handleWrapperOutputChanged,
            Qt::UniqueConnection);
    connect(surface,
            &SurfaceWrapper::minimizedChanged,
            this,
            &MultitaskviewSurfaceModel::handleMinimizedChanged,
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
    disconnectSurface(surface);

    removeReadySurface(surface);
}

void MultitaskviewSurfaceModel::addReadySurface(SurfaceWrapper *surface)
{
    Q_ASSERT_X(surfaceReady(surface),
               __func__,
               "Surface wrapper should be ready before adding to multitaskview model.");
    auto existingIt =
        std::find_if(m_data.begin(), m_data.end(), [surface](const ModelDataPtr &modelData) {
            return modelData->wrapper == surface;
        });
    if (existingIt != m_data.end())
        return;

    disconnect(surface,
               &SurfaceWrapper::normalGeometryChanged,
               this,
               &MultitaskviewSurfaceModel::handleWrapperGeometryChanged);
    disconnect(surface,
               &SurfaceWrapper::geometryChanged,
               this,
               &MultitaskviewSurfaceModel::handleWrapperGeometryChanged);
    if (auto wsurface = surface->surface()) {
        connect(wsurface,
                &WSurface::mappedChanged,
                this,
                &MultitaskviewSurfaceModel::handleSurfaceMappedChanged,
                Qt::UniqueConnection);
    }
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

bool MultitaskviewSurfaceModel::removeReadySurface(SurfaceWrapper *surface)
{
    auto toBeRemovedIt =
        std::find_if(m_data.begin(), m_data.end(), [surface](ModelDataPtr modelData) {
            return modelData->wrapper == surface;
        });
    if (toBeRemovedIt == m_data.end())
        return false;
    int toRemove = std::distance(m_data.begin(), toBeRemovedIt);
    beginRemoveRows({}, toRemove, toRemove);
    m_data.remove(toRemove);
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
    return true;
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

void MultitaskviewSurfaceModel::disconnectSurface(SurfaceWrapper *surface)
{
    disconnect(surface,
               &SurfaceWrapper::ownsOutputChanged,
               this,
               &MultitaskviewSurfaceModel::handleWrapperOutputChanged);
    disconnect(surface,
               &SurfaceWrapper::minimizedChanged,
               this,
               &MultitaskviewSurfaceModel::handleMinimizedChanged);
    disconnect(surface,
               &SurfaceWrapper::normalGeometryChanged,
               this,
               &MultitaskviewSurfaceModel::handleWrapperGeometryChanged);
    disconnect(surface,
               &SurfaceWrapper::geometryChanged,
               this,
               &MultitaskviewSurfaceModel::handleWrapperGeometryChanged);

    if (auto wsurface = surface->surface()) {
        disconnect(wsurface,
                   &WSurface::mappedChanged,
                   this,
                   &MultitaskviewSurfaceModel::handleSurfaceMappedChanged);
    }
}

bool MultitaskviewSurfaceModel::surfaceReady(SurfaceWrapper *surface)
{
    if (surface->type() != SurfaceWrapper::Type::SplashScreen) {
        auto surf = surface->surface();
        if (!surf || !surf->mapped()) {
            return false;
        }
    }
    return surfaceGeometry(surface).isValid();
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
    return workspace()->laterActiveThan(a, b);
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
