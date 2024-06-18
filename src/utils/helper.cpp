// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include <WOutput>
#include <WServer>
#include <WSurfaceItem>
#include <wxdgsurface.h>
#include <wxwaylandsurface.h>

#include <qwbackend.h>
#include <qwcompositor.h>
#include <qwdisplay.h>
#include <qwoutput.h>
#include <qwxdgshell.h>

#include <QAction>
#include <QFile>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>

extern "C" {
#define static
#include <wlr/types/wlr_compositor.h>
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
    auto [output, infoPtr] = getFirstOutputOfSurface(layerSurface);
    if (!output)
        return 0;

    auto exclusiveZone = layerSurface->exclusiveZone();
    auto exclusiveEdge = layerSurface->getExclusiveZoneEdge();

    if (exclusiveZone <= 0 || exclusiveEdge == WLayerSurface::AnchorType::None)
        return false;

    QListIterator<std::tuple<WLayerSurface *, uint32_t, WLayerSurface::AnchorType>> listIter(
        infoPtr->registeredSurfaceList);
    while (listIter.hasNext()) {
        if (std::get<WLayerSurface *>(listIter.next()) == layerSurface)
            return false;
    }

    infoPtr->registeredSurfaceList.append(
        std::make_tuple(layerSurface, exclusiveZone, exclusiveEdge));
    switch (exclusiveEdge) {
        using enum WLayerSurface::AnchorType;
    case Top:
        infoPtr->exclusiveMargins.top += exclusiveZone;
        Q_EMIT topExclusiveMarginChanged();
        break;
    case Bottom:
        infoPtr->exclusiveMargins.bottom += exclusiveZone;
        Q_EMIT bottomExclusiveMarginChanged();
        break;
    case Left:
        infoPtr->exclusiveMargins.left += exclusiveZone;
        Q_EMIT leftExclusiveMarginChanged();
        break;
    case Right:
        infoPtr->exclusiveMargins.right += exclusiveZone;
        Q_EMIT rightExclusiveMarginChanged();
        break;
    default:
        Q_UNREACHABLE();
    }
    return true;
}

bool Helper::unregisterExclusiveZone(WLayerSurface *layerSurface)
{
    auto [output, infoPtr] = getFirstOutputOfSurface(layerSurface);
    if (!output)
        return 0;

    QMutableListIterator<std::tuple<WLayerSurface *, uint32_t, WLayerSurface::AnchorType>> listIter(
        infoPtr->registeredSurfaceList);
    while (listIter.hasNext()) {
        auto [registeredSurface, exclusiveZone, exclusiveEdge] = listIter.next();
        if (registeredSurface == layerSurface) {
            listIter.remove();

            switch (exclusiveEdge) {
                using enum WLayerSurface::AnchorType;
            case Top:
                infoPtr->exclusiveMargins.top -= exclusiveZone;
                Q_EMIT topExclusiveMarginChanged();
                break;
            case Bottom:
                infoPtr->exclusiveMargins.bottom -= exclusiveZone;
                Q_EMIT bottomExclusiveMarginChanged();
                break;
            case Left:
                infoPtr->exclusiveMargins.left -= exclusiveZone;
                Q_EMIT leftExclusiveMarginChanged();
                break;
            case Right:
                infoPtr->exclusiveMargins.right -= exclusiveZone;
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

Margins Helper::getExclusiveMargins(WLayerSurface *layerSurface)
{
    auto [output, infoPtr] = getFirstOutputOfSurface(layerSurface);
    Margins margins;

    if (output) {
        QMutableListIterator<std::tuple<WLayerSurface *, uint32_t, WLayerSurface::AnchorType>>
            listIter(infoPtr->registeredSurfaceList);
        while (listIter.hasNext()) {
            auto [registeredSurface, exclusiveZone, exclusiveEdge] = listIter.next();
            if (registeredSurface == layerSurface)
                break;
            switch (exclusiveEdge) {
                using enum WLayerSurface::AnchorType;
            case Top:
                margins.top += exclusiveZone;
                break;
            case Bottom:
                margins.bottom += exclusiveZone;
                break;
            case Left:
                margins.left += exclusiveZone;
                break;
            case Right:
                margins.right += exclusiveZone;
                break;
            default:
                Q_UNREACHABLE();
            }
        }
    }

    return margins;
}

quint32 Helper::getTopExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [_, infoPtr] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->exclusiveMargins.top;
}

quint32 Helper::getBottomExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [_, infoPtr] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->exclusiveMargins.bottom;
}

quint32 Helper::getLeftExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [_, infoPtr] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->exclusiveMargins.left;
}

quint32 Helper::getRightExclusiveMargin(WToplevelSurface *layerSurface)
{
    auto [_, infoPtr] = getFirstOutputOfSurface(layerSurface);
    if (!infoPtr)
        return 0;
    return infoPtr->exclusiveMargins.right;
}

void Helper::onSurfaceEnterOutput(WToplevelSurface *surface,
                                  WSurfaceItem *surfaceItem,
                                  WOutput *output)
{
    auto *info = getOutputInfo(output);
    info->surfaceList.append(surface);
    info->surfaceItemList.append(surfaceItem);
}

void Helper::onSurfaceLeaveOutput(WToplevelSurface *surface,
                                  WSurfaceItem *surfaceItem,
                                  WOutput *output)
{
    auto *info = getOutputInfo(output);
    info->surfaceList.removeOne(surface);
    info->surfaceItemList.removeOne(surfaceItem);
    // should delete OutputInfo if no surface?
}

Margins Helper::getOutputExclusiveMargins(WOutput *output)
{
    return getOutputInfo(output)->exclusiveMargins;
}

std::pair<WOutput *, OutputInfo *> Helper::getFirstOutputOfSurface(WToplevelSurface *surface)
{
    for (auto zoneInfo : m_outputExclusiveZoneInfo) {
        if (std::get<OutputInfo *>(zoneInfo)->surfaceList.contains(surface))
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
    resizeEdgets = { 0 };
}

void Helper::startMove(WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, int serial)
{
    stopMoveResize();

    Q_UNUSED(serial)

    surfaceItem = shell;
    this->surface = surface;
    this->seat = seat;
    resizeEdgets = { 0 };
    surfacePosOfStartMoveResize = getItemGlobalPosition(surfaceItem);

    setMovingItem(shell);
}

void Helper::startResize(
    WToplevelSurface *surface, WSurfaceItem *shell, WSeat *seat, Qt::Edges edge, int serial)
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
        if (!m_actions.empty() && !m_currentUser.isEmpty()) {
            auto e = static_cast<QKeyEvent *>(event);
            QKeySequence sequence(e->modifiers() | e->key());
            bool isFind = false;
            for (QAction *action : m_actions[m_currentUser]) {
                if (action->shortcut() == sequence) {
                    isFind = true;
                    action->activate(QAction::Trigger);
                }
            }

            if (isFind) {
                return true;
            }
        }
    }

    // Alt+Tab switcher
    // TODO: move to mid handle
    auto e = static_cast<QKeyEvent *>(event);

    switch (e->key()) {
    case Qt::Key_Alt: {
        if (m_switcherOn && event->type() == QKeyEvent::KeyRelease) {
            m_switcherOn = false;
            Q_EMIT switcherOnChanged(false);
            return false;
        }
    } break;
    case Qt::Key_Tab:
    case Qt::Key_Backtab: {
        if (event->type() == QEvent::KeyPress) {
            // switcher would be exclusively disabled when multitask etc is on
            if (e->modifiers().testFlag(Qt::AltModifier) && m_switcherEnabled) {
                if (e->modifiers() == Qt::AltModifier) {
                    Q_EMIT switcherChanged(Switcher::Next);
                    return true;
                } else if (e->modifiers() == (Qt::AltModifier | Qt::ShiftModifier)) {
                    Q_EMIT switcherChanged(Switcher::Previous);
                    return true;
                }
            }
        }
    } break;
    case Qt::Key_BracketLeft:
    case Qt::Key_Delete: {
        if (e->modifiers() == Qt::MetaModifier) {
            Q_EMIT backToNormal();
            Q_EMIT reboot();
            return true;
        }
    } break;
    default: {
    } break;
    }

    if (event->type() == QEvent::KeyPress) {
        auto kevent = static_cast<QKeyEvent *>(event);
        if (QKeySequence(kevent->keyCombination()) == QKeySequence::Quit) {
            // NOTE: 133 exitcode is reset DDM restart limit.
            qApp->exit(133);
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
            QMouseEvent *ev = static_cast<QMouseEvent *>(event);

            if (resizeEdgets == 0) {
                auto increment_pos =
                    ev->globalPosition() - cursor->lastPressedOrTouchDownPosition();
                auto new_pos = surfacePosOfStartMoveResize
                    + surfaceItem->parentItem()->mapFromGlobal(increment_pos);
                surfaceItem->setPosition(new_pos);
            } else {
                auto increment_pos = surfaceItem->parentItem()->mapFromGlobal(
                    ev->globalPosition() - cursor->lastPressedOrTouchDownPosition());
                QRectF geo(surfacePosOfStartMoveResize, surfaceSizeOfStartMoveResize);

                if (resizeEdgets & Qt::LeftEdge)
                    geo.setLeft(geo.left() + increment_pos.x());
                if (resizeEdgets & Qt::TopEdge)
                    geo.setTop(geo.top() + increment_pos.y());

                if (resizeEdgets & Qt::RightEdge)
                    geo.setRight(geo.right() + increment_pos.x());
                if (resizeEdgets & Qt::BottomEdge)
                    geo.setBottom(geo.bottom() + increment_pos.y());

                if (surfaceItem->resizeSurface(geo.size().toSize()))
                    surfaceItem->setPosition(geo.topLeft());
            }

            return true;
        } else if (event->type() == QEvent::MouseButtonRelease
                   || event->type() == QEvent::TouchEnd) {
            stopMoveResize();
        }
    }

    return false;
}

bool Helper::afterHandleEvent(
    WSeat *seat, WSurface *watched, QObject *surfaceItem, QObject *, QInputEvent *event)
{
    Q_UNUSED(seat)

    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin) {
        // surfaceItem is qml type: XdgSurfaceItem or LayerSurfaceItem
        auto toplevelSurface = qobject_cast<WSurfaceItem*>(surfaceItem)->shellSurface();
        if (!toplevelSurface)
            return false;
        Q_ASSERT(toplevelSurface->surface() == watched);
        if (auto *xdgSurface = qobject_cast<WXdgSurface *>(toplevelSurface)) {
            // TODO(waylib): popupSurface should not inherit WToplevelSurface
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
        if (static_cast<QSinglePointEvent *>(event)->isBeginEvent())
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
    qDebug()<<"setActivateSurf"<<newActivate;
    if (newActivate) {
        wl_client *client = newActivate->surface()->handle()->handle()->resource->client;
        pid_t pid;
        uid_t uid;
        gid_t gid;
        wl_client_get_credentials(client, &pid, &uid, &gid);

        QString programName;
        QFile file(QString("/proc/%1/status").arg(pid));
        if (file.open(QFile::ReadOnly)) {
            programName =
                QString(file.readLine()).section(QRegularExpression("([\\t ]*:[\\t ]*|\\n)"), 1, 1);
        }

        if (programName == "dde-desktop") {
            return;
        }
    }

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
        auto output = qobject_cast<QWOutput *>(sender());

        if (newState->state->mode_type == WLR_OUTPUT_STATE_MODE_CUSTOM) {
            const QSize size(newState->state->custom_mode.width,
                             newState->state->custom_mode.height);
            output->setCustomMode(size, newState->state->custom_mode.refresh);
        } else {
            output->setMode(newState->state->mode);
        }

        output->commit();
    }
}

OutputInfo *Helper::getOutputInfo(WOutput *output)
{
    for (const auto &[woutput, infoPtr] : m_outputExclusiveZoneInfo)
        if (woutput == output)
            return infoPtr;
    auto infoPtr = new OutputInfo;
    m_outputExclusiveZoneInfo.append(std::make_pair(output, infoPtr));
    return infoPtr;
}

void Helper::setCurrentUser(const QString &currentUser)
{
    m_currentUser = currentUser;
}

QString Helper::waylandSocket() const
{
    return m_waylandSocket;
}

QString Helper::xwaylandSocket() const
{
    return m_xwaylandSocket;
}

void Helper::setWaylandSocket(const QString &socketFile)
{
    m_waylandSocket = socketFile;

    emit socketFileChanged();
}

void Helper::setXWaylandSocket(const QString &socketFile)
{
    m_xwaylandSocket = socketFile;

    emit socketFileChanged();
}

QString Helper::clientName(Waylib::Server::WSurface *surface) const
{
    wl_client *client = surface->handle()->handle()->resource->client;
    pid_t pid;
    uid_t uid;
    gid_t gid;
    wl_client_get_credentials(client, &pid, &uid, &gid);

    QString programName;
    QFile file(QString("/proc/%1/status").arg(pid));
    if (file.open(QFile::ReadOnly)) {
        programName =
            QString(file.readLine()).section(QRegularExpression("([\\t ]*:[\\t ]*|\\n)"), 1, 1);
    }

    qDebug() << "Program name for PID" << pid << "is" << programName;
    return programName;
}

void Helper::closeSurface(Waylib::Server::WSurface *surface)
{
    if (auto s = Waylib::Server::WXdgSurface::fromSurface(surface)) {
        if (!s->isPopup()) {
            s->handle()->topToplevel()->sendClose();
        }
    } else if (auto s = Waylib::Server::WXWaylandSurface::fromSurface(surface)) {
        s->close();
    }
}

bool Helper::addAction(const QString &user, QAction *action)
{
    if (!m_actions.count(user)) {
        m_actions[user] = {};
    }

    auto find = std::ranges::find_if(m_actions[user], [action](QAction *a) {
        return a->shortcut() == action->shortcut();
    });

    if (find == m_actions[user].end()) {
        m_actions[user].push_back(action);
    }

    return find == m_actions[user].end();
}

void Helper::removeAction(const QString &user, QAction *action)
{
    if (!m_actions.count(user)) {
        return;
    }

    std::erase_if(m_actions[user], [action](QAction *a) {
        return a == action;
    });
}
