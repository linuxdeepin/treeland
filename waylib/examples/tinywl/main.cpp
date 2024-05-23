// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include <WServer>
#include <WOutput>
#include <WSurfaceItem>
#include <wxdgsurface.h>
#include <wrenderhelper.h>
// TODO: Don't use private API
#include <wquickbackend_p.h>

#include <qwbackend.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwlogging.h>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QProcess>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QLoggingCategory>
#include <QKeySequence>

extern "C" {
#define static
#include <wlr/types/wlr_output.h>
#undef static
}

inline QPointF getItemGlobalPosition(QQuickItem *item)
{
    auto parent = item->parentItem();
    return parent ? parent->mapToGlobal(item->position()) : item->position();
}

Helper::Helper(QObject *parent)
    : WSeatEventFilter(parent)
{

}

WSurfaceItem *Helper::resizingItem() const
{
    return m_resizingItem;
}

void Helper::setResizingItem(WSurfaceItem *newResizingItem)
{
    if (m_resizingItem == newResizingItem)
        return;
    m_resizingItem = newResizingItem;
    emit resizingItemChanged();
}

WSurfaceItem *Helper::movingItem() const
{
    return m_movingItem;
}

bool Helper::registerExclusiveZone(WLayerSurface *layerSurface)
{
    auto [ output, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    if (!output)
        return 0;

    auto exclusiveZone = layerSurface->exclusiveZone();
    auto exclusiveEdge = layerSurface->getExclusiveZoneEdge();

    if (exclusiveZone <= 0 || exclusiveEdge == WLayerSurface::AnchorType::None)
        return false;

    QListIterator<std::tuple<WLayerSurface*, uint32_t, WLayerSurface::AnchorType>> listIter(infoPtr->registeredSurfaceList);
    while (listIter.hasNext()) {
        if (std::get<WLayerSurface*>(listIter.next()) == layerSurface)
            return false;
    }

    infoPtr->registeredSurfaceList.append(std::make_tuple(layerSurface, exclusiveZone, exclusiveEdge));
    switch(exclusiveEdge) {
        using enum WLayerSurface::AnchorType;
    case Top:
        infoPtr->m_topExclusiveMargin += exclusiveZone;
        Q_EMIT topExclusiveMarginChanged();
        break;
    case Bottom:
        infoPtr->m_bottomExclusiveMargin += exclusiveZone;
        Q_EMIT bottomExclusiveMarginChanged();
        break;
    case Left:
        infoPtr->m_leftExclusiveMargin += exclusiveZone;
        Q_EMIT leftExclusiveMarginChanged();
        break;
    case Right:
        infoPtr->m_rightExclusiveMargin += exclusiveZone;
        Q_EMIT rightExclusiveMarginChanged();
        break;
    default:
        Q_UNREACHABLE();
    }
    return true;
}

bool Helper::unregisterExclusiveZone(WLayerSurface *layerSurface)
{
    auto [ output, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    if (!output)
        return 0;

    QMutableListIterator<std::tuple<WLayerSurface*, uint32_t, WLayerSurface::AnchorType>> listIter(infoPtr->registeredSurfaceList);
    while (listIter.hasNext()) {
        auto [ registeredSurface, exclusiveZone, exclusiveEdge ] = listIter.next();
        if (registeredSurface == layerSurface) {
            listIter.remove();

            switch(exclusiveEdge) {
                using enum WLayerSurface::AnchorType;
            case Top:
                infoPtr->m_topExclusiveMargin -= exclusiveZone;
                Q_EMIT topExclusiveMarginChanged();
                break;
            case Bottom:
                infoPtr->m_bottomExclusiveMargin -= exclusiveZone;
                Q_EMIT bottomExclusiveMarginChanged();
                break;
            case Left:
                infoPtr->m_leftExclusiveMargin -= exclusiveZone;
                Q_EMIT leftExclusiveMarginChanged();
                break;
            case Right:
                infoPtr->m_rightExclusiveMargin -= exclusiveZone;
                Q_EMIT rightExclusiveMarginChanged();
                break;
            default:
                Q_UNREACHABLE();
            }
            return true;
        }
    }

    return false;
}

QJSValue Helper::getExclusiveMargins(WLayerSurface *layerSurface)
{
    auto [ output, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    QMargins margins{0, 0, 0, 0};

    if (output) {
        QMutableListIterator<std::tuple<WLayerSurface*, uint32_t, WLayerSurface::AnchorType>> listIter(infoPtr->registeredSurfaceList);
        while (listIter.hasNext()) {
            auto [ registeredSurface, exclusiveZone, exclusiveEdge ] = listIter.next();
            if (registeredSurface == layerSurface)
                break;
            switch(exclusiveEdge) {
                using enum WLayerSurface::AnchorType;
            case Top:
                margins.setTop(margins.top() + exclusiveZone);
                break;
            case Bottom:
                margins.setBottom(margins.bottom() + exclusiveZone);
                break;
            case Left:
                margins.setLeft(margins.left() + exclusiveZone);
                break;
            case Right:
                margins.setRight(margins.right() + exclusiveZone);
                break;
            default:
                Q_UNREACHABLE();
            }
        }
    }

    QJSValue jsMargins = qmlEngine(this)->newObject(); // Can't use QMargins in QML
    jsMargins.setProperty("top" , margins.top());
    jsMargins.setProperty("bottom", margins.bottom());
    jsMargins.setProperty("left", margins.left());
    jsMargins.setProperty("right", margins.right());
    return jsMargins;
}

quint32 Helper::getTopExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [ _, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->m_topExclusiveMargin;
}

quint32 Helper::getBottomExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [ _, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->m_bottomExclusiveMargin;
}

quint32 Helper::getLeftExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [ _, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->m_leftExclusiveMargin;
}

quint32 Helper::getRightExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [ _, infoPtr ] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->m_rightExclusiveMargin;
}

void Helper::onSurfaceEnterOutput(WToplevelSurface *surface, WSurfaceItem *surfaceItem, WOutput *output)
{
    auto *info = getOutputInfo(output);
    info->surfaceList.append(surface);
    info->surfaceItemList.append(surfaceItem);
}

void Helper::onSurfaceLeaveOutput(WToplevelSurface *surface, WSurfaceItem *surfaceItem, WOutput *output)
{
    auto *info = getOutputInfo(output);
    info->surfaceList.removeOne(surface);
    info->surfaceItemList.removeOne(surfaceItem);
    // should delete OutputInfo if no surface?
}

std::pair<WOutput*,OutputInfo*> Helper::getFirstOutputOfSurface(WToplevelSurface *surface)
{
    for (auto zoneInfo: m_outputExclusiveZoneInfo) {
        if (std::get<OutputInfo*>(zoneInfo)->surfaceList.contains(surface))
            return zoneInfo;
    }
    return std::make_pair(nullptr, nullptr);
}


void Helper::setMovingItem(WSurfaceItem *newMovingItem)
{
    if (m_movingItem == newMovingItem)
        return;
    m_movingItem = newMovingItem;
    emit movingItemChanged();
}

void Helper::stopMoveResize()
{
    if (surface)
        surface->setResizeing(false);

    setResizingItem(nullptr);
    setMovingItem(nullptr);

    surfaceItem = nullptr;
    surface = nullptr;
    seat = nullptr;
    resizeEdgets = {0};
}

void Helper::startMove(WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, int serial)
{
    stopMoveResize();

    Q_UNUSED(serial)

    surfaceItem = shell;
    this->surface = surface;
    this->seat = seat;
    resizeEdgets = {0};
    surfacePosOfStartMoveResize = getItemGlobalPosition(surfaceItem);

    setMovingItem(shell);
}

void Helper::startResize(WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, Qt::Edges edge, int serial)
{
    stopMoveResize();

    Q_UNUSED(serial)
    Q_ASSERT(edge != 0);

    surfaceItem = shell;
    this->surface = surface;
    this->seat = seat;
    surfacePosOfStartMoveResize = getItemGlobalPosition(surfaceItem);
    surfaceSizeOfStartMoveResize = surfaceItem->size();
    resizeEdgets = edge;

    surface->setResizeing(true);
    setResizingItem(shell);
}

void Helper::cancelMoveResize(WSurfaceItem *shell)
{
    if (surfaceItem != shell)
        return;
    stopMoveResize();
}

bool Helper::startDemoClient(const QString &socket)
{
#ifdef START_DEMO
    QProcess waylandClientDemo;

    waylandClientDemo.setProgram("qml");
    waylandClientDemo.setArguments({"-a", "widget", SOURCE_DIR"/ClientWindow.qml", "-platform", "wayland"});
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("WAYLAND_DISPLAY", socket);

    waylandClientDemo.setProcessEnvironment(env);
    return waylandClientDemo.startDetached();
#endif
    return false;
}

WSurface *Helper::getFocusSurfaceFrom(QObject *object)
{
    auto item = WSurfaceItem::fromFocusObject(object);
    return item ? item->surface() : nullptr;
}

void Helper::allowNonDrmOutputAutoChangeMode(WOutput *output)
{
    connect(output->handle(), &QWOutput::requestState, this, &Helper::onOutputRequeseState);
}

void Helper::enableOutput(WOutput *output)
{
    // Enable on default
    auto qwoutput = output->handle();
    // Don't care for WOutput::isEnabled, must do WOutput::commit here,
    // In order to ensure trigger QWOutput::frame signal, WOutputRenderWindow
    // needs this signal to render next frmae. Because QWOutput::frame signal
    // maybe emit before WOutputRenderWindow::attach, if no commit here,
    // WOutputRenderWindow will ignore this ouptut on render.
    if (!qwoutput->property("_Enabled").toBool()) {
        qwoutput->setProperty("_Enabled", true);

        if (!qwoutput->handle()->current_mode) {
            auto mode = qwoutput->preferredMode();
            if (mode)
                output->setMode(mode);
        }
        output->enable(true);
        bool ok = output->commit();
        Q_ASSERT(ok);
    }
}

bool Helper::beforeDisposeEvent(WSeat *seat, QWindow *watched, QInputEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto kevent = static_cast<QKeyEvent*>(event);
        if (QKeySequence(kevent->keyCombination()) == QKeySequence::Quit) {
            qApp->quit();
            return true;
        }
    }

    if (watched) {
        if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin) {
            seat->setKeyboardFocusTarget(watched);
        } else if (event->type() == QEvent::MouseMove && !seat->focusWindow()) {
            // TouchMove keep focus on first window
            seat->setKeyboardFocusTarget(watched);
        }
    }

    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
        seat->cursor()->setVisible(true);
    } else if (event->type() == QEvent::TouchBegin) {
        seat->cursor()->setVisible(false);
    }

    if (surfaceItem && (seat == this->seat || this->seat == nullptr)) {
        // for move resize
        if (Q_LIKELY(event->type() == QEvent::MouseMove || event->type() == QEvent::TouchUpdate)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent*>(event);

            if (resizeEdgets == 0) {
                auto increment_pos = ev->globalPosition() - cursor->lastPressedOrTouchDownPosition();
                auto new_pos = surfacePosOfStartMoveResize + surfaceItem->parentItem()->mapFromGlobal(increment_pos);
                surfaceItem->setPosition(new_pos);
            } else {
                auto increment_pos = surfaceItem->parentItem()->mapFromGlobal(ev->globalPosition() - cursor->lastPressedOrTouchDownPosition());
                QRectF geo(surfacePosOfStartMoveResize, surfaceSizeOfStartMoveResize);

                if (resizeEdgets & Qt::LeftEdge)
                    geo.setLeft(geo.left() + increment_pos.x());
                if (resizeEdgets & Qt::TopEdge)
                    geo.setTop(geo.top() + increment_pos.y());

                if (resizeEdgets & Qt::RightEdge)
                    geo.setRight(geo.right() + increment_pos.x());
                if (resizeEdgets & Qt::BottomEdge)
                    geo.setBottom(geo.bottom() + increment_pos.y());

                if (surface->checkNewSize(geo.size().toSize())) {
                    surfaceItem->setPosition(geo.topLeft());
                    surfaceItem->resizeSurface(geo.size().toSize());
                }
            }

            return true;
        } else if (event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::TouchEnd) {
            stopMoveResize();
        }
    }

    return false;
}

bool Helper::afterHandleEvent(WSeat *seat, WSurface *watched, QObject *surfaceItem, QObject *, QInputEvent *event)
{
    Q_UNUSED(seat)

    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin) {
        // surfaceItem is qml type: XdgSurfaceItem or LayerSurfaceItem
        auto *toplevelSurface = qvariant_cast<WToplevelSurface*>(surfaceItem->property("surface"));

        if (!toplevelSurface)
            return false;
        Q_ASSERT(toplevelSurface->surface() == watched);
        if (auto *xdgSurface = qvariant_cast<WXdgSurface*>(surfaceItem->property("surface"))) {
            // TODO: popupSurface should not inherit WToplevelSurface
            if (xdgSurface->isPopup()) {
                return false;
            }
        }
        setActivateSurface(toplevelSurface);
    }

    return false;
}

bool Helper::unacceptedEvent(WSeat *, QWindow *, QInputEvent *event)
{
    if (event->isSinglePointEvent()) {
        if (static_cast<QSinglePointEvent*>(event)->isBeginEvent())
            setActivateSurface(nullptr);
    }

    return false;
}

WToplevelSurface *Helper::activatedSurface() const
{
    return m_activateSurface;
}

void Helper::setActivateSurface(WToplevelSurface *newActivate)
{
    if (m_activateSurface == newActivate)
        return;

    if (newActivate && newActivate->doesNotAcceptFocus())
        return;

    if (m_activateSurface) {
        if (newActivate) {
            if (m_activateSurface->keyboardFocusPriority() > newActivate->keyboardFocusPriority())
                return;
        } else {
            if (m_activateSurface->keyboardFocusPriority() > 0)
                return;
        }

        m_activateSurface->setActivate(false);
    }
    m_activateSurface = newActivate;
    if (newActivate)
        newActivate->setActivate(true);
    Q_EMIT activatedSurfaceChanged();
}

void Helper::onOutputRequeseState(wlr_output_event_request_state *newState)
{
    if (newState->state->committed & WLR_OUTPUT_STATE_MODE) {
        auto output = qobject_cast<QWOutput*>(sender());

        if (newState->state->mode_type == WLR_OUTPUT_STATE_MODE_CUSTOM) {
            const QSize size(newState->state->custom_mode.width, newState->state->custom_mode.height);
            output->setCustomMode(size, newState->state->custom_mode.refresh);
        } else {
            output->setMode(newState->state->mode);
        }

        output->commit();
    }
}

OutputInfo* Helper::getOutputInfo(WOutput *output)
{
    for (const auto &[woutput, infoPtr]: m_outputExclusiveZoneInfo)
        if (woutput == output)
            return infoPtr;
    auto infoPtr = new OutputInfo;
    m_outputExclusiveZoneInfo.append(std::make_pair(output, infoPtr));
    return infoPtr;
}

int main(int argc, char *argv[]) {
    WRenderHelper::setupRendererBackend();

    QWLog::init();
    WServer::initializeQPA();
//    QQuickStyle::setStyle("Material");

    QGuiApplication::setAttribute(Qt::AA_UseOpenGLES);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine waylandEngine;
    QString cursorThemeName = getenv("XCURSOR_THEME");
    waylandEngine.rootContext()->setContextProperty("cursorThemeName", cursorThemeName);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    waylandEngine.loadFromModule("Tinywl", "Main");
#else
    waylandEngine.load(QUrl(u"qrc:/Tinywl/Main.qml"_qs));
#endif
    WServer *server = waylandEngine.rootObjects().first()->findChild<WServer*>();
    Q_ASSERT(server);
    Q_ASSERT(server->isRunning());

    auto backend = server->findChild<WQuickBackend*>();
    Q_ASSERT(backend);

    // multi output
//    qobject_cast<QWMultiBackend*>(backend->backend())->forEachBackend([] (wlr_backend *backend, void *) {
//        if (auto x11 = QWX11Backend::from(backend))
//            x11->createOutput();
//    }, nullptr);

    return app.exec();
}
