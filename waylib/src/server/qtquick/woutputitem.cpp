// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutputitem.h"
#include "woutputitem_p.h"
#include "woutput.h"
#include "wquickoutputlayout.h"
#include "wquickcursor.h"
#include "private/wglobal_p.h"

#include <qwoutput.h>
#include <qwoutputlayout.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

WOutputItemAttached::WOutputItemAttached(QObject *parent)
    : QObject(parent)
{

}

WOutputItem *WOutputItemAttached::item() const
{
    return m_positioner;
}

void WOutputItemAttached::setItem(WOutputItem *positioner)
{
    if (m_positioner == positioner)
        return;
    m_positioner = positioner;
    Q_EMIT itemChanged();
}

#define DATA_OF_WOUPTUT "_WOutputItem"
class Q_DECL_HIDDEN WOutputItemPrivate : public WObjectPrivate
{
public:
    WOutputItemPrivate(WOutputItem *qq)
        : WObjectPrivate(qq)
    {

    }
    ~WOutputItemPrivate() {
        clearCursors();

        if (layout)
            layout->remove(q_func());
        if (output)
            output->setProperty(DATA_OF_WOUPTUT, QVariant());
    }

    void initForOutput();

    void updateImplicitSize() {
        W_Q(WOutputItem);

        q->privateImplicitWidthChanged();
        q->privateImplicitHeightChanged();

        q->resetWidth();
        q->resetHeight();
    }

    void clearCursors();
    void updateCursors();
    void updateCursorsVisible();
    void updateCursorVisible(WOutputCursor *cursor);

    WOutputCursor *getCursorItemBy(WCursor *cursor) const;

    W_DECLARE_PUBLIC(WOutputItem)
    QPointer<WOutput> output;
    QPointer<WQuickOutputLayout> layout;
    qreal devicePixelRatio = 1.0;

    QQmlComponent *cursorDelegate = nullptr;
    QList<std::pair<WCursor*, WOutputCursor*>> cursors;
    QMetaObject::Connection updateCursorsConnection;
};

void WOutputItemPrivate::initForOutput()
{
    W_Q(WOutputItem);
    Q_ASSERT(output);

    if (layout)
        layout->add(q);

    output->safeConnect(&WOutput::transformedSizeChanged, q, [this] {
        updateImplicitSize();
    });

    output->safeConnect(&WOutput::aboutToBeInvalidated, q, [this, q] {
        if (layout) {
            layout->remove(q);
            layout = nullptr;
        }
    });

    updateImplicitSize();

    clearCursors();
    if (updateCursorsConnection)
        QObject::disconnect(updateCursorsConnection);
    if (output) {
        updateCursorsConnection = QObject::connect(output, SIGNAL(cursorListChanged()),
                                                   q, SLOT(updateCursors()));
        updateCursors();
    }
}

void WOutputItemPrivate::clearCursors()
{
    for (auto i : std::as_const(cursors))
        i.second->deleteLater();
    cursors.clear();
}

void WOutputItemPrivate::updateCursors()
{
    if (!output || !cursorDelegate)
        return;

    W_Q(WOutputItem);

    QList<std::pair<WCursor*, WOutputCursor*>> tmpCursors;
    tmpCursors.reserve(cursors.size());
    bool cursorsChanged = false;

    const auto cursorList = output->cursorList();
    for (WCursor *cursor : cursorList) {
        auto *oc = getCursorItemBy(cursor);
        if (!oc) {
            oc = new WOutputCursor(q);
            oc->m_cursor = cursor;
            oc->m_visible = cursor->isVisible();
            updateCursorVisible(oc);

            Q_ASSERT(q->window());
            auto obj = cursorDelegate->createWithInitialProperties({
                {"outputCurosr", QVariant::fromValue(oc)},
                {"parent", QVariant::fromValue(q->window()->contentItem())},
            }, qmlContext(q));
            oc->item = qobject_cast<QQuickItem*>(obj);

            if (!oc->item)
                qFatal("Cursor delegate must is Item");

            // ensure following this to destroy, because QQuickItem::setParentItem
            // is not auto add the child item to QObject's children.
            oc->item->setParent(oc);
            QQmlEngine::setObjectOwnership(oc->item, QQmlEngine::CppOwnership);
            Q_ASSERT(oc->item->parentItem() == q->window()->contentItem());
            oc->item->setZ(qreal(WOutputLayout::Layer::Cursor));
            cursorsChanged = true;

            QObject::connect(cursor, &WCursor::positionChanged, oc, [this, oc] {
                updateCursorVisible(oc);
            });
        }

        tmpCursors.append(std::make_pair(cursor, oc));
    }

    std::swap(tmpCursors, cursors);
    // clean needless cursors
    for (auto i : std::as_const(tmpCursors)) {
        if (cursors.contains(i))
            continue;
        i.second->setParent(nullptr);
        i.second->deleteLater();
        cursorsChanged = true;
    }

    if (cursorsChanged)
        Q_EMIT q->cursorItemsChanged();
}

void WOutputItemPrivate::updateCursorsVisible()
{
    for (const auto &i : std::as_const(cursors))
        updateCursorVisible(i.second);
}

void WOutputItemPrivate::updateCursorVisible(WOutputCursor *cursor)
{
    if (!cursor->cursor()->isVisible()) {
        cursor->setVisible(false);
        return;
    }

    W_QC(WOutputItem);
    const QRectF globalGeometry(q->globalPosition(), q->size());
    cursor->setVisible(globalGeometry.contains(cursor->cursor()->position()));
}

WOutputCursor *WOutputItemPrivate::getCursorItemBy(WCursor *cursor) const
{
    for (auto i : std::as_const(cursors))
        if (i.first == cursor)
            return i.second;
    return nullptr;
}

WOutputCursor::WOutputCursor(WOutputItem *parent)
    : QObject(parent)
{

}

bool WOutputCursor::visible() const
{
    return m_visible;
}

void WOutputCursor::setVisible(bool newVisible)
{
    if (m_visible == newVisible)
        return;
    m_visible = newVisible;
    Q_EMIT visibleChanged();
}

WCursor *WOutputCursor::cursor() const
{
    return m_cursor;
}

WOutputItem::WOutputItem(QQuickItem *parent)
    : WQuickObserver(parent)
    , WObject(*new WOutputItemPrivate(this))
{
    connect(this, &WOutputItem::maybeGlobalPositionChanged, this, [this] {
        d_func()->updateCursorsVisible();
    });
}

WOutputItem::~WOutputItem()
{

}

WOutputItemAttached *WOutputItem::qmlAttachedProperties(QObject *target)
{
    auto output = qobject_cast<WOutput*>(target);
    if (!output)
        return nullptr;
    auto attached = new WOutputItemAttached(output);
    attached->setItem(getOutputItem(output));

    return attached;
}

WOutputItem *WOutputItem::getOutputItem(WOutput *output)
{
    return qvariant_cast<WOutputItem*>(output->property(DATA_OF_WOUPTUT)) ;
}

WOutput *WOutputItem::output() const
{
    W_D(const WOutputItem);
    return d->output.get();
}

inline static WOutputItemAttached *getAttached(WOutput *output)
{
    return output->findChild<WOutputItemAttached*>(QString(), Qt::FindDirectChildrenOnly);
}

void WOutputItem::setOutput(WOutput *newOutput)
{
    W_D(WOutputItem);

    Q_ASSERT(!d->output || !newOutput);
    d->output = newOutput;

    if (newOutput) {
        if (auto attached = getAttached(newOutput)) {
            attached->setItem(this);
        } else {
            newOutput->setProperty(DATA_OF_WOUPTUT, QVariant::fromValue(this));
        }
    }

    if (isComponentComplete()) {
        if (newOutput) {
            d->initForOutput();
        }
    }
    Q_EMIT outputChanged();
}

WQuickOutputLayout *WOutputItem::layout() const
{
    Q_D(const WOutputItem);
    return d->layout.get();
}

void WOutputItem::setLayout(WQuickOutputLayout *layout)
{
    Q_D(WOutputItem);

    if (d->layout == layout)
        return;

    if (d->layout)
        d->layout->remove(this);

    d->layout = layout;
    if (isComponentComplete() && d->layout && d->output)
        d->layout->add(this);

    Q_EMIT layoutChanged();
}

qreal WOutputItem::devicePixelRatio() const
{
    W_DC(WOutputItem);
    return d->devicePixelRatio;
}

void WOutputItem::setDevicePixelRatio(qreal newDevicePixelRatio)
{
    W_D(WOutputItem);

    if (qFuzzyCompare(d->devicePixelRatio, newDevicePixelRatio))
        return;
    d->devicePixelRatio = newDevicePixelRatio;

    if (d->output)
        d->updateImplicitSize();

    Q_EMIT devicePixelRatioChanged();
}

QQmlComponent *WOutputItem::cursorDelegate() const
{
    W_DC(WOutputItem);
    return d->cursorDelegate;
}

void WOutputItem::setCursorDelegate(QQmlComponent *delegate)
{
    W_D(WOutputItem);
    if (d->cursorDelegate == delegate)
        return;

    d->cursorDelegate = delegate;
    d->clearCursors();

    Q_EMIT cursorDelegateChanged();
}

QList<QQuickItem *> WOutputItem::cursorItems() const
{
    W_DC(WOutputItem);

    QList<QQuickItem *> items;
    items.reserve(d->cursors.size());

    for (auto i : std::as_const(d->cursors))
        items.append(i.second->item);

    return items;
}

void WOutputItem::classBegin()
{
    W_D(WOutputItem);

    QQuickItem::classBegin();
}

void WOutputItem::componentComplete()
{
    W_D(WOutputItem);

    if (d->output) {
        d->initForOutput();
    }

    WQuickObserver::componentComplete();
}

void WOutputItem::releaseResources()
{
    W_D(WOutputItem);

    WQuickObserver::releaseResources();
}

void WOutputItem::itemChange(ItemChange change, const ItemChangeData &data)
{
    QQuickItem::itemChange(change, data);
}

qreal WOutputItem::getImplicitWidth() const
{
    W_DC(WOutputItem);
    return d->output->transformedSize().width() / d->devicePixelRatio;
}

qreal WOutputItem::getImplicitHeight() const
{
    W_DC(WOutputItem);
    return d->output->transformedSize().height() / d->devicePixelRatio;
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_woutputitem.cpp"
