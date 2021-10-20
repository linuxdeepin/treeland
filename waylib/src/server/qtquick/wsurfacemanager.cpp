/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "wsurfacemanager.h"
#include "private/wsurfacelayout_p.h"
#include "wserver.h"
#include "wbackend.h"
#include "woutput.h"
#include "wquickrendercontrol.h"
#include "wsurfaceitem.h"
#include "wsurface.h"
#include "wxdgsurface.h"
#include "woutputlayout.h"
#include "wsurfacelayout.h"
#include "wthreadutils.h"
#include "wseat.h"

#include <QQuickView>
#include <QQmlEngine>
#include <QQmlContext>
#include <QGuiApplication>

#include <private/qquickitem_p.h>
#include <private/qquickanchors_p.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class ItemChangeListener : public QQuickItemChangeListener
{
public:
    ItemChangeListener(WSurfaceManagerPrivate *manager)
        : m_manager(manager) {}

private:
    void itemGeometryChanged(QQuickItem *item, QQuickGeometryChange,
                             const QRectF &oldGeometry ) override;

    WSurfaceManagerPrivate *m_manager;
};

class InitEvent : public QEvent {
public:
    enum Type {
        InitOutput = QEvent::User + 1,
        InitSurface = QEvent::User + 2
    };

    InitEvent(WOutput *output)
        : QEvent(static_cast<QEvent::Type>(InitOutput))
    {
        data.output = output;
    }
    InitEvent(WSurface *surface)
        : QEvent(static_cast<QEvent::Type>(InitSurface))
    {
        data.surface = surface;
    }

    union {
        WOutput *output;
        WSurface *surface;
    } data;
};

class WSurfaceManagerPrivate : public WSurfaceLayoutPrivate
{
public:
    WSurfaceManagerPrivate(WSurfaceManager *qq)
        : WSurfaceLayoutPrivate(qq)
    {

    }
    ~WSurfaceManagerPrivate() {
        initialized = false;
        auto windows = output2WindowMap.values();
        auto items = surface2ItemMap.values();

        output2WindowMap.clear();
        surface2ItemMap.clear();
        item2SurfaceMap.clear();

        Q_FOREACH(auto item, items) {
            if (itemChangeListener)
                QQuickItemPrivate::get(item)->removeItemChangeListener(itemChangeListener.data(),
                                                                       QQuickItemPrivate::Geometry);
            item->deleteLater();
        }
        Q_FOREACH(auto window, windows)
            window->deleteLater();
    }

    inline ItemChangeListener *ensureListener() {
        if (!itemChangeListener)
            itemChangeListener.reset(new ItemChangeListener(this));
        return itemChangeListener.data();
    }

    void init();

    void initOutput(WOutput *output);
    void initSurface(WSurface *surface);

    void doInitOutput(WOutput *output);
    void doInitSurface(WSurface *surface);

    void surfaceMapTo(WSurface *surface, WOutput *output);
    void onItemPositionChanged(QQuickItem *item);
    void onItemSizeChanged(QQuickItem *item);

    void updateSurfaceItemPosition(WSurface *surface);
    void updateSurfaceItemSize(WSurface *surface);

    W_DECLARE_PUBLIC(WSurfaceManager)

    bool initialized = false;
    WServer *server = nullptr;
    QHash<WOutput*, QQuickWindow*> output2WindowMap;
    QHash<WSurface*, QQuickItem*> surface2ItemMap;
    QHash<QQuickItem*, WSurface*> item2SurfaceMap;

    QScopedPointer<ItemChangeListener> itemChangeListener;
    QPointer<WSurface> activateSurface;
};

void WSurfaceManagerPrivate::init()
{
    initialized = true;
    Q_FOREACH(auto output, outputLayout->outputList()) {
        initOutput(output);
    }

    Q_FOREACH(auto surface, surfaceList) {
        initSurface(surface);
    }

    W_Q(WSurfaceManager);
    QObject::connect(outputLayout, &WOutputLayout::outputAdded,
                     q, &WSurfaceManager::handleOutputAdded);
    QObject::connect(outputLayout, &WOutputLayout::outputRemoved,
                     q, &WSurfaceManager::handleOutputRemoved);
}

void WSurfaceManagerPrivate::initOutput(WOutput *output)
{
    outputLayout->add(output);

    if (initialized) {
#ifdef D_WM_MAIN_THREAD_QTQUICK
        QCoreApplication::postEvent(q_func(), new InitEvent(output));
#else
        doInitOutput(output);
#endif
    }
}

void WSurfaceManagerPrivate::initSurface(WSurface *surface)
{
    if (initialized) {
#ifdef D_WM_MAIN_THREAD_QTQUICK
        QCoreApplication::postEvent(q_func(), new InitEvent(surface));
#else
        doInitSurface(surface);
#endif
    }
}

void WSurfaceManagerPrivate::doInitOutput(WOutput *output)
{
    W_Q(WSurfaceManager);
    WQuickRenderControl *rc = new WQuickRenderControl();
    auto window = q->createWindow(rc, output);
    Q_ASSERT(window);
    output2WindowMap[output] = window;
    rc->setParent(window);
    output->attach(rc);

    window->setGeometry(QRect(output->position(), output->effectiveSize()));

    QObject::connect(output, &WOutput::positionChanged,
                     window, QOverload<const QPoint&>::of(&QQuickWindow::setPosition));
    QObject::connect(output, &WOutput::effectiveSizeChanged,
                     window, QOverload<const QSize&>::of(&QQuickWindow::resize));
}

void WSurfaceManagerPrivate::doInitSurface(WSurface *surface)
{
    W_Q(WSurfaceManager);

    QObject::connect(surface, &WSurface::effectivePositionChanged,
                     q, [this, surface] {
        updateSurfaceItemPosition(surface);
    }, Qt::DirectConnection);
    QObject::connect(surface, &WSurface::effectiveSizeChanged,
                     q, [this, surface] {
        updateSurfaceItemSize(surface);
    }, Qt::DirectConnection);
}

void WSurfaceManagerPrivate::surfaceMapTo(WSurface *surface, WOutput *output)
{
    W_Q(WSurfaceManager);
    QQuickItem *parent = nullptr;

    if (auto parent_surface = surface->parentSurface()) {
        parent = surface2ItemMap.value(parent_surface);
    } else {
        parent = q->layerItem(output->attachedWindow(), q->surfaceLayer(surface));
    }
    Q_ASSERT(parent);
    auto item = surface2ItemMap.value(surface);
    item->setParent(parent);
    item->setParentItem(parent);
}

void WSurfaceManagerPrivate::onItemPositionChanged(QQuickItem *item)
{
    Q_UNUSED(item)
}

void WSurfaceManagerPrivate::onItemSizeChanged(QQuickItem *item)
{
    Q_UNUSED(item)
}

void WSurfaceManagerPrivate::updateSurfaceItemPosition(WSurface *surface)
{
    auto item = surface2ItemMap.value(surface);
    if (!item)
        return;
    item->setPosition(surface->effectivePosition());
}

void WSurfaceManagerPrivate::updateSurfaceItemSize(WSurface *surface)
{
    auto item = surface2ItemMap.value(surface);
    if (!item)
        return;
    item->setSize(surface->effectiveSize());
}

WSurfaceManager::WSurfaceManager(WServer *server, WOutputLayout *layout, QObject *parent)
    : WSurfaceLayout(*new WSurfaceManagerPrivate(this), layout, parent)
{
    d_func()->server = server;
}

WServer *WSurfaceManager::server() const
{
    W_DC(WSurfaceManager);
    return d->server;
}

void WSurfaceManager::initialize()
{
    W_D(WSurfaceManager);
    Q_ASSERT(!d->initialized);

#ifdef D_WM_MAIN_THREAD_QTQUICK
    d->init();
#else
    d->server->threadUtil()->run(this, d, &WSurfaceManagerPrivate::init);
#endif
}

void WSurfaceManager::add(WSurface *surface)
{
    WSurfaceLayout::add(surface);
    W_D(WSurfaceManager);
    d->initSurface(surface);
}

void WSurfaceManager::remove(WSurface *surface)
{
    WSurfaceLayout::remove(surface);
    W_D(WSurfaceManager);

    if (d->initialized)
        unmap(surface);
}

void WSurfaceManager::setActivateSurface(WSurface *surface)
{
    W_D(WSurfaceManager);
    if (surface == d->activateSurface)
        return;
    if (d->activateSurface)
        d->activateSurface->notifyEndState(WSurface::State::Activate);
    d->activateSurface = surface;
    if (surface)
        surface->notifyBeginState(WSurface::State::Activate);
}

void WSurfaceManager::map(WSurface *surface)
{
    W_D(WSurfaceManager);
    QQuickItem *item = createSurfaceItem(surface);
    Q_ASSERT(item);
    QQuickItemPrivate::get(item)->addItemChangeListener(d->ensureListener(),
                                                        QQuickItemPrivate::Geometry);
    d->surface2ItemMap[surface] = item;
    d->item2SurfaceMap[item] = surface;
    d->surfaceMapTo(surface, surface->attachedOutput());
    item->setPosition(surface->effectivePosition());
    item->setSize(surface->effectiveSize());
}

void WSurfaceManager::unmap(WSurface *surface)
{
    W_D(WSurfaceManager);
    auto item = d->surface2ItemMap.take(surface);
    if (item) {
        d->item2SurfaceMap.remove(item);
        item->deleteLater();
    }
}

bool WSurfaceManager::isMapped(WSurface *surface) const
{
    W_DC(WSurfaceManager);
    return d->surface2ItemMap.value(surface);
}

bool WSurfaceManager::setOutput(WSurface *surface, WOutput *output)
{
    if (!WSurfaceLayout::setOutput(surface, output))
        return false;

    W_D(WSurfaceManager);

    if (d->surface2ItemMap.value(surface))
        d->surfaceMapTo(surface, output);
    return true;
}

void WSurfaceManager::requestMaximize(WSurface *surface)
{
    W_D(WSurfaceManager);

    auto item = d->surface2ItemMap.value(surface);
    Q_ASSERT(item);
    auto layer = item->parentItem();
    Q_ASSERT(layer);
    surface->setProperty("_d_normal_pos", surface->position());
    surface->setProperty("_d_normal_size", surface->size());
    surface->notifyBeginState(WSurface::State::Maximize);
    QQuickItemPrivate::get(item)->anchors()->setFill(layer);
}

void WSurfaceManager::requestUnmaximize(WSurface *surface)
{
    W_D(WSurfaceManager);

    auto item = d->surface2ItemMap.value(surface);
    Q_ASSERT(item);
    QQuickItemPrivate::get(item)->anchors()->resetFill();
    surface->resize(surface->property("_d_normal_size").toSize());
    setPosition(surface, surface->property("_d_normal_pos").toPointF());
    surface->notifyEndState(WSurface::State::Maximize);
}

void WSurfaceManager::requestActivate(WSurface *surface, WSeat *seat)
{
    if (surface->testAttribute(WSurface::Attribute::DoesNotAcceptFocus))
        return;
    seat->setKeyboardFocusTarget(surface);
    setActivateSurface(surface);
}

bool WSurfaceManager::event(QEvent *event)
{
    W_D(WSurfaceManager);
    if (event->type() == static_cast<int>(InitEvent::InitOutput)) {
        d->doInitOutput(static_cast<InitEvent*>(event)->data.output);
    } else if (event->type() == static_cast<int>(InitEvent::InitSurface)) {
        d->doInitSurface(static_cast<InitEvent*>(event)->data.surface);
    }

    return QObject::event(event);
}

void WSurfaceManager::handleOutputAdded(WOutput *output)
{
    W_D(WSurfaceManager);
    d->initOutput(output);
}

void WSurfaceManager::handleOutputRemoved(WOutput *output)
{
    W_D(WSurfaceManager);

    if (d->initialized) {
        auto window = d->output2WindowMap.take(output);
        if (window)
            window->deleteLater();
    }
}

QQuickWindow *WSurfaceManager::createWindow(QQuickRenderControl *rc, WOutput *output)
{
    Q_UNUSED(output)
    auto window = new QQuickWindow(rc);
//    window->setClearBeforeRendering(false);
    window->setColor(Qt::gray);
    return window;
}

QQuickItem *WSurfaceManager::createSurfaceItem(WSurface *surface)
{
    auto item = new WSurfaceItem();
    item->setSurface(surface);
    return item;
}

WSurfaceManager::Layer WSurfaceManager::surfaceLayer(WSurface *surface) const
{
    Q_UNUSED(surface)
    return Layer::Window;
}

QQuickItem *WSurfaceManager::layerItem(QQuickWindow *window, Layer layer) const
{
    Q_UNUSED(layer)
    return window->contentItem();
}

void ItemChangeListener::itemGeometryChanged(QQuickItem *item, QQuickGeometryChange changes, const QRectF &)
{
    if (changes.positionChange()) {
        m_manager->onItemPositionChanged(item);
    }

    if (changes.sizeChange()) {
        m_manager->onItemSizeChanged(item);
    }
}

WAYLIB_SERVER_END_NAMESPACE

#include "moc_wsurfacemanager.cpp"
