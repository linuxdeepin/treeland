// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "multitaskview.h"

#include "helper.h"
#include "surfacecontainer.h"
#include "treelandconfig.h"

#include <QtConcurrentMap>

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
    if (surface)
        Q_EMIT surface->requestForceActive();
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
    if (!m_surfaceListModel || m_layoutArea.isEmpty())
        return;
    beginResetModel();
    m_data.clear();
    std::transform(
        m_surfaceListModel->surfaces().begin(),
        m_surfaceListModel->surfaces().end(),
        std::back_inserter(m_data),
        [this](SurfaceWrapper *wrapper) -> SurfaceModelData {
            return { wrapper, wrapper->geometry().translated(-layoutArea().topLeft()), false };
        });
    endResetModel();
    m_modelReady = true;
    Q_EMIT modelReadyChanged();
}

void MultitaskviewSurfaceModel::setSurfaceListModel(SurfaceListModel *surfaceListModel)
{
    if (surfaceListModel == m_surfaceListModel)
        return;
    if (m_surfaceListModel)
        disconnect(m_surfaceListModel,
                   &SurfaceListModel::dataChanged,
                   this,
                   &MultitaskviewSurfaceModel::calcLayout);
    m_surfaceListModel = surfaceListModel;
    if (m_surfaceListModel)
        connect(m_surfaceListModel,
                &SurfaceListModel::dataChanged,
                this,
                &MultitaskviewSurfaceModel::calcLayout,
                Qt::UniqueConnection);
    initializeModel();
    Q_EMIT surfaceListModelChanged();
}

SurfaceListModel *MultitaskviewSurfaceModel::surfaceListModel() const
{
    return m_surfaceListModel;
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
        return QVariant::fromValue(m_data[r].wrapper);
    case GeometryRole:
        return QVariant::fromValue(m_data[r].geometry);
    case PaddingRole:
        return QVariant::fromValue(m_data[r].padding);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MultitaskviewSurfaceModel::roleNames() const
{
    return QHash<int, QByteArray>{ { SurfaceWrapperRole, "wrapper" },
                                   { GeometryRole, "geometry" },
                                   { PaddingRole, "padding" } };
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
    auto maxWindowHeight = std::min(layoutArea().height(),
                                    static_cast<qreal>(TreelandConfig::ref().normalWindowHeight()));
    auto minWindowHeight = TreelandConfig::ref().minMultitaskviewSurfaceHeight();
    auto windowHeightStep = TreelandConfig::ref().windowHeightStep();
    auto rowH = maxWindowHeight;
    while (rowH > minWindowHeight) {
        if (tryLayout(rowH)) {
            break;
        }
        rowH -= windowHeightStep;
    }
    if (rowH < minWindowHeight) {
        tryLayout(minWindowHeight, true);
    }
    calcDisplayPos();
    Q_EMIT dataChanged(index(0), index(rowCount() - 1), { GeometryRole, PaddingRole });
    Q_EMIT rowsChanged();
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

bool MultitaskviewSurfaceModel::tryLayout(qreal rowH, bool ignoreOverlap)
{
    int nrows = 1;
    qreal acc = 0;
    auto availWidth = layoutArea().width();
    auto availHeight = layoutArea().height();
    if (availWidth <= 0)
        return false;
    QList<QList<SurfaceModelData *>> rowstmp;
    QList<SurfaceModelData *> currow;
    for (auto &data : m_data) {
        auto surface = data.wrapper;
        auto whRatio = surface->width() / surface->height();
        data.padding = surface->height() < (rowH - 2 * CellPadding);
        auto curW = std::min(availWidth,
                             whRatio * std::min(rowH - 2 * CellPadding, surface->height())
                                 + 2 * CellPadding);
        data.geometry.setWidth(curW - 2 * CellPadding);
        auto newAcc = acc + curW;
        if (newAcc <= availWidth) {
            acc = newAcc;
            currow.append(&data);
        } else if (newAcc / availWidth > LoadFactor) {
            acc = curW;
            nrows++;
            rowstmp.append(currow);
            currow = { &data };
        } else {
            // Just scale the last element
            curW = availWidth - acc;
            data.geometry.setWidth(curW - 2 * CellPadding);
            currow.append(&data);
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
            [](SurfaceModelData *data) {
                return data->geometry.width() + 2 * CellPadding;
            },
            [](qreal &acc, const qreal &cur) {
                acc += cur;
            });
        auto curX = hCenter - totW / 2 + CellPadding;
        for (auto window : row) {
            window->geometry.moveLeft(curX);
            window->geometry.moveTop(curY);
            window->geometry.setHeight(m_rowHeight - 2 * CellPadding);
            curX += window->geometry.width() + 2 * CellPadding;
        }
        curY += m_rowHeight;
    }
    m_contentHeight = curY;
}

uint MultitaskviewSurfaceModel::rows() const
{
    return m_rows.count();
}
