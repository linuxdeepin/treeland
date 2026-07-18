// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "helper.h"

#include "qwxdgshell.h"
#include "seatsmanager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <qnamespace.h>
#ifdef EXT_SESSION_LOCK_V1
#include "wsessionlock.h"
#include "wsessionlockmanager.h"
#include "core/lockscreen.h"
#endif
#ifndef DISABLE_DDM
#include "core/lockscreen.h"
#endif
#include "common/treelandlogging.h"
#include "core/layersurfacecontainer.h"
#include "core/qmlengine.h"
#include "core/rootsurfacecontainer.h"
#include "core/shellhandler.h"
#include "core/treeland.h"
#include "core/windowpicker.h"
#include "greeter/greeterproxy.h"
#include "greeter/sessionmodel.h"
#include "greeter/usermodel.h"
#include "input/inputdevice.h"
#include "inputmanager.h"
#include "interfaces/multitaskviewinterface.h"
#include "modules/capture/capture.h"
#include "modules/dde-shell/ddeshellattached.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "modules/ddm/ddminterfacev1.h"
#include "modules/input-manager/inputmanagerinterfacev1.h"
#include "modules/keyboard-state-notify/keyboardstatenotifymanagerinterfacev1.h"
#include "modules/output-manager/outputmanagement.h"
#include "modules/personalization/personalizationmanagerinterfacev1.h"
#include "modules/resource/treelandremotesource.h"
#include "modules/screensaver/screensaverinterfacev1.h"
#include "modules/shortcut/shortcutcontroller.h"
#include "modules/shortcut/shortcutmanager.h"
#include "modules/shortcut/shortcutrunner.h"
#include "modules/wallpaper-color/wallpapercolorinterfacev1.h"
#include "output/output.h"
#include "output/outputconfigstate.h"
#include "output/outputlifecyclemanager.h"
#include "outputconfig.hpp"
#include "session/session.h"
#include "surface/surfacecontainer.h"
#include "surface/surfacewrapper.h"
#include "treelandconfig.hpp"
#include "treelanduserconfig.hpp"
#include "utils/cmdline.h"
#include "utils/fpsdisplaymanager.h"
#include "wallpaper/wallpapermanager.h"
#include "wallpapershellinterfacev1.h"
#include "workspace/workspace.h"

#include <rhi/qrhi.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include <WBackend>
#include <WForeignToplevel>
#include <WOutput>
#include <WServer>
#include <WSurfaceItem>
#include <WCursor>
#include <WXdgOutput>
#include <wayland-util.h>
#include <wcursorshapemanagerv1.h>
#include <wextimagecapturesourcev1impl.h>
#include <wlayersurface.h>
#include <woutputhelper.h>
#include <woutputitem.h>
#include <woutputlayout.h>
#include <woutputmanagerv1.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wqmlcreator.h>
#include <wquickcursor.h>
#include <wrenderhelper.h>
#include <wseat.h>
#include <wsecuritycontextmanager.h>
#include <wsocket.h>
#include <wtoplevelsurface.h>
#include <wxdgpopupsurface.h>
#include <wxdgshell.h>
#include <wxdgtoplevelsurface.h>
#include <wxdgtopleveltagmanager.h>
#include <wxwayland.h>
#include <wxwaylandsurface.h>

#include <qwallocator.h>
#include <qwalphamodifierv1.h>
#include <qwbackend.h>
#include <qwbuffer.h>
#include <qwcompositor.h>
#include <qwdatacontrolv1.h>
#include <qwdatadevice.h>
#include <qwdisplay.h>
#include <qwdrm.h>
#include <qwextdatacontrolv1.h>
#include <qwextforeigntoplevelimagecapturesourcemanagerv1.h>
#include <qwextforeigntoplevellistv1.h>
#include <qwextimagecapturesourcev1.h>
#include <qwextimagecopycapturev1.h>
#include <qwfractionalscalemanagerv1.h>
#include <qwgammacontorlv1.h>
#include <qwidleinhibitv1.h>
#include <qwidlenotifyv1.h>
#include <qwinputdevice.h>
#include <qwlayershellv1.h>
#include <qwlogging.h>
#include <qwoutput.h>
#include <qwoutputpowermanagementv1.h>
#include <qwrenderer.h>
#include <qwscreencopyv1.h>
#include <qwsession.h>
#include <qwsubcompositor.h>
#include <qwviewporter.h>
#include <qwxdgforeignregistry.h>
#include <qwxdgforeignv2.h>
#include <qwxwayland.h>
#include <qwxwaylandsurface.h>

#include <DGuiApplicationHelper>

#include <QAction>
#include <QByteArray>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QGuiApplication>
#include <QKeySequence>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QPointer>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScopedValueRollback>
#include <QStyleHints>
#include <QThreadPool>

#include <functional>
#include <pwd.h>
#include <unistd.h>
#include <utility>

#define EXT_DATA_CONTROL_MANAGER_V1_VERSION 1
#define WLR_FRACTIONAL_SCALE_V1_VERSION 1

namespace {

uint32_t xwaylandWindowId(SurfaceWrapper *wrapper)
{
    if (!wrapper || wrapper->type() != SurfaceWrapper::Type::XWayland)
        return 0;

    auto *surface = qobject_cast<WXWaylandSurface *>(wrapper->shellSurface());
    if (!surface || !surface->handle() || !surface->handle()->handle())
        return 0;

    return surface->handle()->handle()->window_id;
}

WXWaylandSurface *xwaylandSurfaceForWrapper(SurfaceWrapper *wrapper)
{
    if (!wrapper || wrapper->type() != SurfaceWrapper::Type::XWayland)
        return nullptr;

    return qobject_cast<WXWaylandSurface *>(wrapper->shellSurface());
}

uint32_t xwaylandWindowId(WXWaylandSurface *surface)
{
    if (!surface || !surface->handle() || !surface->handle()->handle())
        return 0;

    return surface->handle()->handle()->window_id;
}

const char *xwaylandInputModelName(WXWaylandSurface::InputModel model)
{
    switch (model) {
    case WXWaylandSurface::InputModelNone:
        return "none";
    case WXWaylandSurface::InputModelPassive:
        return "passive";
    case WXWaylandSurface::InputModelLocal:
        return "local";
    case WXWaylandSurface::InputModelGlobal:
        return "global";
    }
    return "unknown";
}

bool isXWaylandPopupPointerEvent(const QInputEvent *event)
{
    if (!event)
        return false;

    switch (event->type()) {
    case QEvent::HoverEnter:
    case QEvent::HoverMove:
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::Wheel:
        return true;
    default:
        return false;
    }
}

bool isXWaylandPointerButtonEvent(const QInputEvent *event)
{
    if (!event)
        return false;

    return event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::MouseButtonRelease;
}

bool xwaylandPopupPointerEventNeedsButtonSequence(const QInputEvent *event)
{
    if (!event)
        return false;

    switch (event->type()) {
    case QEvent::MouseButtonRelease:
        return true;
    case QEvent::MouseMove:
        return static_cast<const QMouseEvent *>(event)->buttons() != Qt::NoButton;
    default:
        return false;
    }
}

bool isXWaylandPopupPointerEnterOrMove(const QInputEvent *event)
{
    if (!event)
        return false;

    switch (event->type()) {
    case QEvent::HoverEnter:
    case QEvent::HoverMove:
    case QEvent::MouseMove:
        return true;
    default:
        return false;
    }
}

enum class X11GlobalRequestFallbackReason {
    None,
    EventTarget,
    ParentWrapper,
    ResolvedParentWrapper,
    X11ParentChain,
    ActivePointerOwner,
};

const char *x11GlobalRequestFallbackReasonName(X11GlobalRequestFallbackReason reason)
{
    switch (reason) {
    case X11GlobalRequestFallbackReason::None:
        return "none";
    case X11GlobalRequestFallbackReason::EventTarget:
        return "event-target";
    case X11GlobalRequestFallbackReason::ParentWrapper:
        return "parent-wrapper";
    case X11GlobalRequestFallbackReason::ResolvedParentWrapper:
        return "resolved-parent-wrapper";
    case X11GlobalRequestFallbackReason::X11ParentChain:
        return "x11-parent-chain";
    case X11GlobalRequestFallbackReason::ActivePointerOwner:
        return "active-pointer-owner";
    }

    return "unknown";
}

struct X11RequestGeometryInfo
{
    bool valid = false;
    QRectF rawGeometry;
    QRectF effectiveGeometry;
    QPointF unwrappedParentOffset;
    uint32_t directParentWindowId = 0;
    QRectF directParentGeometry;
    uint32_t effectiveParentWindowId = 0;
    QRectF effectiveParentGeometry;
    int unwrappedParentDepth = 0;
    bool usedUnwrappedParentOffset = false;
    bool parentChainTruncated = false;
};

struct X11GlobalRequestFallbackDecision
{
    bool hasX11RequestPosition = false;
    bool allowed = false;
    bool sceneEdgeAllowed = false;
    const char *rejectReason = "not-evaluated";
    QPointF sceneLocalPos;
    QPointF globalLocalPos;
    QRectF requestGeometry;
    bool sceneContains = false;
    bool globalContains = false;
    qreal sceneMissDistance = 0.0;
    qreal missThreshold = 0.0;
    qreal sceneEdgeHitThreshold = 0.0;
};

struct XWaylandPopupGlobalHit
{
    bool valid = false;
    bool contains = false;
    bool itemContains = false;
    bool inputRegionContains = false;
    QPointF cursorPos;
    QPointF localPos;
    QPointF itemGlobalTopLeft;
    QRectF itemGlobalGeometry;
};

QPointF pointerEventScenePosition(const QInputEvent *event)
{
    if (!event || !event->isSinglePointEvent())
        return QPointF();

    return static_cast<const QSinglePointEvent *>(event)->scenePosition();
}

QPointF pointerEventGlobalPosition(const QInputEvent *event)
{
    if (!event || !event->isSinglePointEvent())
        return QPointF();

    return static_cast<const QSinglePointEvent *>(event)->globalPosition();
}

QPointF pointerCursorGlobalPosition(WSeat *seat, const QInputEvent *event)
{
    if (seat && seat->cursor())
        return seat->cursor()->position();

    return pointerEventGlobalPosition(event);
}

qreal xwaylandSurfaceSizeRatio(SurfaceWrapper *wrapper)
{
    auto *surfaceItem = wrapper ? wrapper->surfaceItem() : nullptr;
    if (!surfaceItem || surfaceItem->surfaceSizeRatio() <= 0.0)
        return 1.0;

    return surfaceItem->surfaceSizeRatio();
}

bool xwaylandSurfaceReady(WXWaylandSurface *surface)
{
    return surface && !surface->isInvalidated() && surface->handle()
        && surface->handle()->handle();
}

X11RequestGeometryInfo xwaylandRequestGeometryInfo(SurfaceWrapper *wrapper)
{
    X11RequestGeometryInfo info;

    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient())
        return info;

    auto *xwaylandSurface = xwaylandSurfaceForWrapper(wrapper);
    if (!xwaylandSurfaceReady(xwaylandSurface))
        return info;

    const QRectF requestRect(xwaylandSurface->requestConfigureGeometry());
    if (!requestRect.isValid() || requestRect.isEmpty())
        return info;

    info.valid = true;
    info.rawGeometry = requestRect;
    info.effectiveGeometry = requestRect;

    auto *parent = xwaylandSurface->parentXWaylandSurface();
    for (int depth = 0; parent && depth < 32; ++depth) {
        if (parent == xwaylandSurface || !xwaylandSurfaceReady(parent)) {
            info.parentChainTruncated = true;
            break;
        }

        const QRectF parentGeometry(parent->geometry());
        if (depth == 0) {
            info.directParentWindowId = xwaylandWindowId(parent);
            info.directParentGeometry = parentGeometry;
        }

        if (parent->surface())
            break;

        info.unwrappedParentOffset += parentGeometry.topLeft();
        info.effectiveParentWindowId = xwaylandWindowId(parent);
        info.effectiveParentGeometry = parentGeometry;
        info.unwrappedParentDepth = depth + 1;

        auto *nextParent = parent->parentXWaylandSurface();
        if (nextParent == parent) {
            info.parentChainTruncated = true;
            break;
        }
        parent = nextParent;
    }

    if (!info.unwrappedParentOffset.isNull()) {
        info.effectiveGeometry.translate(info.unwrappedParentOffset);
        info.usedUnwrappedParentOffset = true;
    }

    return info;
}

bool xwaylandRequestPointerLocalPosition(SurfaceWrapper *wrapper,
                                         WSeat *seat,
                                         const QInputEvent *event,
                                         QPointF *localPos,
                                         bool *contains,
                                         QRectF *requestGeometry,
                                         QPointF *globalLocalPos = nullptr,
                                         bool *globalContains = nullptr,
                                         bool *usedEffectiveRequestGeometry = nullptr,
                                         X11RequestGeometryInfo *requestInfo = nullptr)
{
    if (localPos)
        *localPos = QPointF();
    if (contains)
        *contains = false;
    if (requestGeometry)
        *requestGeometry = QRectF();
    if (globalLocalPos)
        *globalLocalPos = QPointF();
    if (globalContains)
        *globalContains = false;
    if (usedEffectiveRequestGeometry)
        *usedEffectiveRequestGeometry = false;
    if (requestInfo)
        *requestInfo = X11RequestGeometryInfo();

    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient() || !event
        || !event->isSinglePointEvent()) {
        return false;
    }

    const X11RequestGeometryInfo info = xwaylandRequestGeometryInfo(wrapper);
    if (requestInfo)
        *requestInfo = info;
    if (!info.valid)
        return false;

    const QRectF rawRequestRect = info.rawGeometry;
    const QRectF effectiveRequestRect = info.effectiveGeometry;

    const qreal surfaceSizeRatio = xwaylandSurfaceSizeRatio(wrapper);
    const QPointF scenePosition = pointerEventScenePosition(event);
    const QPointF scaledScenePosition(scenePosition.x() * surfaceSizeRatio,
                                      scenePosition.y() * surfaceSizeRatio);
    const QPointF rawLocal = scaledScenePosition - rawRequestRect.topLeft();
    const QPointF effectiveLocal = scaledScenePosition - effectiveRequestRect.topLeft();
    const QPointF cursorGlobalPosition = pointerCursorGlobalPosition(seat, event);
    const QPointF rawGlobalLocal = cursorGlobalPosition - rawRequestRect.topLeft();
    const QPointF effectiveGlobalLocal =
        cursorGlobalPosition - effectiveRequestRect.topLeft();
    const QRectF rawLocalRequestRect(QPointF(), rawRequestRect.size());
    const QRectF effectiveLocalRequestRect(QPointF(), effectiveRequestRect.size());
    const bool rawContains = rawLocalRequestRect.contains(rawLocal);
    const bool effectiveContains = effectiveLocalRequestRect.contains(effectiveLocal);
    const bool rawGlobalContains = rawLocalRequestRect.contains(rawGlobalLocal);
    const bool effectiveGlobalContains =
        effectiveLocalRequestRect.contains(effectiveGlobalLocal);
    const bool useEffectiveRequestGeometry =
        !rawContains && info.usedUnwrappedParentOffset;

    if (localPos)
        *localPos = useEffectiveRequestGeometry ? effectiveLocal : rawLocal;
    if (contains)
        *contains = useEffectiveRequestGeometry ? effectiveContains : rawContains;
    if (requestGeometry)
        *requestGeometry = useEffectiveRequestGeometry ? effectiveRequestRect : rawRequestRect;
    if (globalLocalPos)
        *globalLocalPos =
            useEffectiveRequestGeometry ? effectiveGlobalLocal : rawGlobalLocal;
    if (globalContains)
        *globalContains =
            useEffectiveRequestGeometry ? effectiveGlobalContains : rawGlobalContains;
    if (usedEffectiveRequestGeometry)
        *usedEffectiveRequestGeometry = useEffectiveRequestGeometry;

    return true;
}

XWaylandPopupGlobalHit xwaylandPopupGlobalHitTest(SurfaceWrapper *wrapper,
                                                  WSeat *seat,
                                                  const QInputEvent *event)
{
    XWaylandPopupGlobalHit hit;

    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient() || !event
        || !event->isSinglePointEvent()) {
        return hit;
    }

    auto *surfaceItem = wrapper->surfaceItem();
    auto *eventItem = surfaceItem ? surfaceItem->eventItem() : nullptr;
    if (!eventItem || !eventItem->window())
        return hit;

    hit.valid = true;
    hit.cursorPos = pointerCursorGlobalPosition(seat, event);
    hit.localPos = eventItem->mapFromGlobal(hit.cursorPos);
    hit.itemContains = eventItem->contains(hit.localPos);
    hit.inputRegionContains =
        wrapper->surface() ? wrapper->surface()->inputRegionContains(hit.localPos) : false;
    hit.contains = hit.itemContains || hit.inputRegionContains;

    hit.itemGlobalTopLeft = eventItem->mapToGlobal(QPointF());
    const QPointF itemGlobalBottomRight =
        eventItem->mapToGlobal(QPointF(eventItem->width(), eventItem->height()));
    hit.itemGlobalGeometry = QRectF(hit.itemGlobalTopLeft, itemGlobalBottomRight).normalized();

    return hit;
}

qreal distanceOutsideRect(const QRectF &rect, const QPointF &point)
{
    if (!rect.isValid() || rect.isEmpty())
        return 0.0;

    const qreal dx = point.x() < rect.left()
        ? rect.left() - point.x()
        : (point.x() > rect.right() ? point.x() - rect.right() : 0.0);
    const qreal dy = point.y() < rect.top()
        ? rect.top() - point.y()
        : (point.y() > rect.bottom() ? point.y() - rect.bottom() : 0.0);

    return qMax(dx, dy);
}

QPointF boundedPointInRect(const QRectF &rect, const QPointF &point)
{
    if (!rect.isValid() || rect.isEmpty())
        return point;

    constexpr qreal edgeEpsilon = 0.001;
    const qreal maxX = rect.right() > rect.left()
        ? qMax(rect.left(), rect.right() - edgeEpsilon)
        : rect.right();
    const qreal maxY = rect.bottom() > rect.top()
        ? qMax(rect.top(), rect.bottom() - edgeEpsilon)
        : rect.bottom();

    return QPointF(qBound(rect.left(), point.x(), maxX),
                   qBound(rect.top(), point.y(), maxY));
}

qreal x11GlobalRequestFallbackMissThreshold()
{
    const QStyleHints *styleHints = QGuiApplication::styleHints();
    return styleHints ? qMax<qreal>(styleHints->startDragDistance(), 10.0) : 10.0;
}

qreal x11RequestSceneEdgeHitThreshold()
{
    return 2.0;
}

bool x11RequestSceneEdgeHitAllowed(X11GlobalRequestFallbackReason fallbackReason)
{
    switch (fallbackReason) {
    case X11GlobalRequestFallbackReason::ParentWrapper:
    case X11GlobalRequestFallbackReason::ResolvedParentWrapper:
    case X11GlobalRequestFallbackReason::X11ParentChain:
    case X11GlobalRequestFallbackReason::ActivePointerOwner:
        return true;
    case X11GlobalRequestFallbackReason::None:
    case X11GlobalRequestFallbackReason::EventTarget:
        return false;
    }

    return false;
}

X11GlobalRequestFallbackDecision x11GlobalRequestFallbackDecision(
    X11GlobalRequestFallbackReason fallbackReason,
    bool itemContains,
    bool inputRegionContains,
    bool hasX11RequestPosition,
    const QRectF &requestGeometry,
    const QPointF &sceneLocalPos,
    bool sceneContains,
    const QPointF &globalLocalPos,
    bool globalContains)
{
    X11GlobalRequestFallbackDecision decision;
    decision.hasX11RequestPosition = hasX11RequestPosition;
    decision.requestGeometry = requestGeometry;
    decision.sceneLocalPos = sceneLocalPos;
    decision.globalLocalPos = globalLocalPos;
    decision.sceneContains = sceneContains;
    decision.globalContains = globalContains;
    decision.missThreshold = x11GlobalRequestFallbackMissThreshold();
    decision.sceneEdgeHitThreshold =
        qMin(decision.missThreshold, x11RequestSceneEdgeHitThreshold());

    if (fallbackReason == X11GlobalRequestFallbackReason::None) {
        decision.rejectReason = "unrelated";
        return decision;
    }

    if (itemContains || inputRegionContains) {
        decision.rejectReason = "item-or-input-region-hit";
        return decision;
    }

    if (!hasX11RequestPosition) {
        decision.rejectReason = "no-x11-request";
        return decision;
    }

    if (sceneContains) {
        decision.rejectReason = "scene-request-hit";
        return decision;
    }

    if (!globalContains) {
        decision.rejectReason = "global-request-miss";
        return decision;
    }

    const QRectF localRequestRect(QPointF(), requestGeometry.size());
    decision.sceneMissDistance = distanceOutsideRect(localRequestRect, sceneLocalPos);
    if (decision.sceneMissDistance <= decision.missThreshold) {
        if (decision.sceneMissDistance <= decision.sceneEdgeHitThreshold
            && x11RequestSceneEdgeHitAllowed(fallbackReason)) {
            decision.allowed = true;
            decision.sceneEdgeAllowed = true;
            decision.rejectReason = "scene-edge-hit";
            return decision;
        }

        decision.rejectReason = "scene-edge-miss";
        return decision;
    }

    decision.allowed = true;
    decision.rejectReason = "allowed";
    return decision;
}

bool popupPointerLocalPosition(SurfaceWrapper *wrapper,
                               WSeat *seat,
                               const QInputEvent *event,
                               QPointF *localPos,
                               bool *contains,
                               bool *usedX11RequestPosition = nullptr,
                               X11GlobalRequestFallbackReason x11GlobalFallbackReason =
                                   X11GlobalRequestFallbackReason::None,
                               bool *usedX11GlobalRequestPosition = nullptr,
                               bool *usedX11EffectiveRequestGeometry = nullptr)
{
    if (localPos)
        *localPos = QPointF();
    if (contains)
        *contains = false;
    if (usedX11RequestPosition)
        *usedX11RequestPosition = false;
    if (usedX11GlobalRequestPosition)
        *usedX11GlobalRequestPosition = false;
    if (usedX11EffectiveRequestGeometry)
        *usedX11EffectiveRequestGeometry = false;

    if (!wrapper || !wrapper->surfaceItem() || !event || !event->isSinglePointEvent())
        return false;

    auto *eventItem = wrapper->surfaceItem()->eventItem();
    if (!eventItem)
        return false;

    const QPointF local = eventItem->mapFromScene(pointerEventScenePosition(event));
    const bool itemContains = eventItem->contains(local);
    const bool inputRegionContains =
        !itemContains && wrapper->surface() ? wrapper->surface()->inputRegionContains(local) : false;
    bool localContains = itemContains || inputRegionContains;

    if (localPos)
        *localPos = local;
    if (contains)
        *contains = localContains;

    if (!localContains) {
        QPointF x11LocalPos;
        QPointF x11GlobalLocalPos;
        bool x11Contains = false;
        bool x11GlobalContains = false;
        bool x11UsedEffectiveRequestGeometry = false;
        QRectF x11RequestGeometry;
        const bool hasX11RequestPosition =
            xwaylandRequestPointerLocalPosition(wrapper,
                                                seat,
                                                event,
                                                &x11LocalPos,
                                                &x11Contains,
                                                &x11RequestGeometry,
                                                &x11GlobalLocalPos,
                                                &x11GlobalContains,
                                                &x11UsedEffectiveRequestGeometry);
        if (hasX11RequestPosition && x11Contains) {
            if (localPos)
                *localPos = x11LocalPos;
            if (contains)
                *contains = true;
            if (usedX11RequestPosition)
                *usedX11RequestPosition = true;
            if (usedX11EffectiveRequestGeometry)
                *usedX11EffectiveRequestGeometry = x11UsedEffectiveRequestGeometry;
        } else {
            const auto globalFallback = x11GlobalRequestFallbackDecision(x11GlobalFallbackReason,
                                                                         itemContains,
                                                                         inputRegionContains,
                                                                         hasX11RequestPosition,
                                                                         x11RequestGeometry,
                                                                         x11LocalPos,
                                                                         x11Contains,
                                                                         x11GlobalLocalPos,
                                                                         x11GlobalContains);
            if (globalFallback.allowed) {
                if (localPos) {
                    *localPos = globalFallback.sceneEdgeAllowed
                        ? boundedPointInRect(QRectF(QPointF(), x11RequestGeometry.size()),
                                             x11LocalPos)
                        : x11GlobalLocalPos;
                }
                if (contains)
                    *contains = true;
                if (usedX11RequestPosition)
                    *usedX11RequestPosition = true;
                if (usedX11GlobalRequestPosition)
                    *usedX11GlobalRequestPosition = !globalFallback.sceneEdgeAllowed;
                if (usedX11EffectiveRequestGeometry)
                    *usedX11EffectiveRequestGeometry = x11UsedEffectiveRequestGeometry;
            }
        }
    }

    return true;
}

} // namespace

static QByteArray readWindowProperty(xcb_connection_t *connection,
                                     xcb_window_t win,
                                     xcb_atom_t atom,
                                     xcb_atom_t type)
{
    QByteArray data;
    int offset = 0;
    int remaining = 0;

    do {
        xcb_get_property_cookie_t cookie =
            xcb_get_property(connection, false, win, atom, type, offset, 1024);
        xcb_get_property_reply_t *reply = xcb_get_property_reply(connection, cookie, NULL);
        if (!reply)
            break;

        remaining = 0;

        if (reply->type == type) {
            int len = xcb_get_property_value_length(reply);
            char *datas = (char *)xcb_get_property_value(reply);
            data.append(datas, len);
            remaining = reply->bytes_after;
            offset += len;
        }

        free(reply);
    } while (remaining > 0);

    return data;
}

static void runWhenOutputConfigInitialized(OutputConfig *config,
                                           QObject *context,
                                           std::function<void()> callback)
{
    if (!config || !context) {
        return;
    }

    if (config->isInitializeSucceeded()) {
        callback();
        return;
    }

    QObject::connect(config,
                     &OutputConfig::configInitializeSucceed,
                     context,
                     [callback = std::move(callback)] { callback(); });
}

static void runWhenTreelandConfigInitialized(TreelandConfig *config,
                                             QObject *context,
                                             std::function<void()> callback)
{
    if (!config || !context) {
        return;
    }

    if (config->isInitializeSucceeded() || config->isInitializeFailed()) {
        callback();
        return;
    }

    auto sharedCallback = std::make_shared<std::function<void()>>(std::move(callback));
    QObject::connect(config,
                     &TreelandConfig::configInitializeSucceed,
                     context,
                     [sharedCallback] { (*sharedCallback)(); });
    QObject::connect(config,
                     &TreelandConfig::configInitializeFailed,
                     context,
                     [sharedCallback] { (*sharedCallback)(); });
}

static bool hasSavedOutputState(OutputConfig *config)
{
    return config && (!config->enabledIsDefaultValue()
                      || !config->widthIsDefaultValue()
                      || !config->heightIsDefaultValue()
                      || !config->refreshIsDefaultValue()
                      || !config->scaleIsDefaultValue()
                      || !config->transformIsDefaultValue()
                      || !config->adaptiveSyncEnabledIsDefaultValue());
}

static bool outputMatchesId(Output *output, const QString &outputId)
{
    return output && output->output() && output->output()->isEnabled()
        && WallpaperManager::getOutputId(output) == outputId;
}

static bool currentPrimaryMatchesId(RootSurfaceContainer *rootContainer, const QString &outputId)
{
    return rootContainer && outputMatchesId(rootContainer->primaryOutput(), outputId);
}

Helper *Helper::m_instance = nullptr;

Helper::Helper(QObject *parent)
    : WSeatEventFilter(parent)
    , m_sessionManager(new SessionManager(this))
    , m_wallpaperManager(new WallpaperManager(this))
    , m_renderWindow(new WOutputRenderWindow(this))
    , m_server(new WServer(this))
    , m_rootSurfaceContainer(new RootSurfaceContainer(m_renderWindow->contentItem()))
    , m_inputManager(new InputManager(this))
{
    m_isDDMDisplay = qEnvironmentVariableIsSet("DDM_DISPLAY_MANAGER");
    Q_ASSERT(!m_instance);
    m_instance = this;

    Q_ASSERT(!m_config);
    m_config.reset(TreelandUserConfig::createByName("org.deepin.dde.treeland.user",
                                              "org.deepin.dde.treeland",
                                              "/dde")); // will update user path in Helper::init
    m_globalConfig.reset(TreelandConfig::create("org.deepin.dde.treeland",
                                                      QString()));

    m_renderWindow->setColor(Qt::black);
    m_rootSurfaceContainer->setFlag(QQuickItem::ItemIsFocusScope, true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    m_rootSurfaceContainer->setFocusPolicy(Qt::StrongFocus);
#endif

    m_shellHandler = new ShellHandler(m_rootSurfaceContainer, m_server);
    connect(m_shellHandler->workspace(),
            &Workspace::workspaceAdded,
            m_wallpaperManager,
            &WallpaperManager::syncAddWorkspace);
    tryInitRemoteSource();

    m_outputConfigState = new OutputConfigState(this);
    m_outputLifecycleManager =
        new OutputLifecycleManager(m_rootSurfaceContainer, m_outputConfigState, this);

#ifdef EXT_SESSION_LOCK_V1
    m_lockScreenGraceTimer = new QTimer(this);
    m_lockScreenGraceTimer->setInterval(300);
    m_lockScreenGraceTimer->setSingleShot(true);
#endif

    m_workspaceScaleAnimation = new QPropertyAnimation(m_shellHandler->workspace(), "scale", this);
    m_workspaceOpacityAnimation =
        new QPropertyAnimation(m_shellHandler->workspace(), "opacity", this);

    m_workspaceScaleAnimation->setDuration(1000);
    m_workspaceOpacityAnimation->setDuration(1000);
    m_workspaceScaleAnimation->setEasingCurve(QEasingCurve::OutExpo);
    m_workspaceOpacityAnimation->setEasingCurve(QEasingCurve::OutExpo);

    connect(m_renderWindow,
            &QQuickWindow::activeFocusItemChanged,
            this,
            &Helper::onRenderWindowActiveFocusItemChanged);

    // Connect to systemd-logind's PrepareForSleep signal for hibernate blackout
    // Also connect to SessionNew signal for logging purposes
    QDBusConnection::systemBus().connect(
        "org.freedesktop.login1",
        "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager",
        "SessionNew",
        this,
        SLOT(onSessionNew(QString,QDBusObjectPath))
    );
}

Helper::~Helper()
{
    Q_ASSERT(m_instance == this);
    m_instance = nullptr;
    if (m_renderWindow) {
        m_renderWindow->disconnect();
    }

    if (m_backend) {
        m_backend->disconnect();
    }

    m_currentEventSeat = nullptr;
    // destroy before m_rootSurfaceContainer
    delete m_shellHandler;
    if (m_rootSurfaceContainer) {
        for (auto s : std::as_const(m_rootSurfaceContainer->surfaces())) {
            if (auto c = s->container())
                c->removeSurface(s);
        }
        delete m_rootSurfaceContainer;
    }
}

Helper *Helper::instance()
{
    return m_instance;
}

TreelandUserConfig *Helper::config()
{
    return m_config.get();
}

TreelandConfig *Helper::globalConfig()
{
    return m_globalConfig.get();
}

void Helper::syncPaletteTypeWithWindowThemeType(int32_t themeType)
{
    auto *guiHelper = DTK_GUI_NAMESPACE::DGuiApplicationHelper::instance();
    if (!guiHelper) {
        qCCritical(lcTlConfig) << "DGuiApplicationHelper instance not available, cannot sync "
                                      "palette type with window theme type.";
        return;
    }

    qCDebug(lcTlConfig) << "Syncing palette type with window theme type:" << themeType;

    switch (themeType) {
    case 2:
        guiHelper->setPaletteType(Dtk::Gui::DGuiApplicationHelper::DarkType);
        break;
    case 1:
        guiHelper->setPaletteType(Dtk::Gui::DGuiApplicationHelper::LightType);
        break;
    default:
        qCWarning(lcTlConfig) << "Unknown windowThemeType:" << themeType
                                  << ", fallback to light.";
        guiHelper->setPaletteType(Dtk::Gui::DGuiApplicationHelper::LightType);
        break;
    }
}

void Helper::tryInitRemoteSource()
{
    if (m_treelandRemoteSource)
        return;
    if (m_globalConfig->debugSource()) {
        m_treelandRemoteSource = new TreelandRemoteSource(this);
    }
}

bool Helper::isNvidiaCardPresent()
{
    auto rhi = m_renderWindow->rhi();

    if (!rhi)
        return false;

    QString deviceName = rhi->driverInfo().deviceName;
    qCDebug(lcTlCore) << "Graphics Device:" << deviceName;

    return deviceName.contains("NVIDIA", Qt::CaseInsensitive);
}

void Helper::setWorkspaceVisible(bool visible)
{
    for (auto *surface : std::as_const(m_rootSurfaceContainer->surfaces())) {
        if (surface->type() == SurfaceWrapper::Type::Layer) {
            surface->setHideByLockScreen(m_currentMode == CurrentMode::LockScreen);
        }
    }

    if (m_noAnimation) {
        m_shellHandler->workspace()->setOpacity(visible ? 1.0 : 0.0);
        m_shellHandler->workspace()->setScale(visible ? 1.0 : 1.4);
        return;
    }

    if (visible) {
        m_workspaceScaleAnimation->stop();
        m_workspaceScaleAnimation->setStartValue(m_shellHandler->workspace()->scale());
        m_workspaceScaleAnimation->setEndValue(1.0);
        m_workspaceScaleAnimation->start();

        m_workspaceOpacityAnimation->stop();
        m_workspaceOpacityAnimation->setStartValue(m_shellHandler->workspace()->opacity());
        m_workspaceOpacityAnimation->setEndValue(1.0);
        m_workspaceOpacityAnimation->start();
    } else {
        m_workspaceScaleAnimation->stop();
        m_workspaceScaleAnimation->setStartValue(m_shellHandler->workspace()->scale());
        m_workspaceScaleAnimation->setEndValue(1.4);
        m_workspaceScaleAnimation->start();

        m_workspaceOpacityAnimation->stop();
        m_workspaceOpacityAnimation->setStartValue(m_shellHandler->workspace()->opacity());
        m_workspaceOpacityAnimation->setEndValue(0.0);
        m_workspaceOpacityAnimation->start();
    }
}

QmlEngine *Helper::qmlEngine() const
{
    return qobject_cast<QmlEngine *>(::qmlEngine(this));
}

WOutputRenderWindow *Helper::window() const
{
    return m_renderWindow;
}

SessionManager *Helper::sessionManager() const
{
    return m_sessionManager;
}

ShellHandler *Helper::shellHandler() const
{
    return m_shellHandler;
}

Workspace *Helper::workspace() const
{
    return m_shellHandler->workspace();
}

void Helper::onOutputAdded(WOutput *output)
{
    // TODO: 应该让helper发出Output的信号，每个需要output的单元单独connect。
    allowNonDrmOutputAutoChangeMode(output);
    Output *o = nullptr;

    bool shouldRestoreCopyMode = (m_mode == OutputMode::Extension && m_outputConfigState
                                  && m_outputConfigState->shouldRestoreCopyMode()
                                  && m_outputList.size() >= 1); // This output + existing outputs

    if (shouldRestoreCopyMode) {
        Output *primaryOutput = m_rootSurfaceContainer->primaryOutput();
        if (!primaryOutput) {
            qCWarning(lcTlCore) << "Cannot restore Copy Mode: no primary output available";
            o = createNormalOutput(output);
        } else {
            const auto &allSurfaces = getWorkspaceSurfaces();
            applyCopyModeToOutputs(primaryOutput, allSurfaces);
            o = createCopyOutput(output, primaryOutput);
        }
    } else if (m_mode == OutputMode::Extension || !m_rootSurfaceContainer->primaryOutput()) {
        o = createNormalOutput(output);
    } else if (m_mode == OutputMode::Copy) {
        o = createCopyOutput(output, m_rootSurfaceContainer->primaryOutput());
    }
    m_outputList.append(o);
    // Handle primary output restoration via lifecycle manager
    if (m_outputLifecycleManager) {
        m_outputLifecycleManager->setMode(m_mode == OutputMode::Extension
                                              ? OutputLifecycleManager::Mode::Extension
                                              : OutputLifecycleManager::Mode::Copy);
        m_outputLifecycleManager->onScreenAdded(o);
    }

    o->enable();

    auto publishOutput = [this, output, outputObject = QPointer<Output>(o)] {
        if (!outputObject) {
            return;
        }

        m_outputManager->newOutput(output);
        m_wallpaperManager->ensureWallpaperConfigForOutput(outputObject);
    };
    auto restoreOutputConfig = [this, output, outputObject = QPointer<Output>(o), publishOutput] {
        auto publish = qScopeGuard(publishOutput);
        if (!outputObject || m_mode == OutputMode::Copy) {
            return;
        }

        auto *config = outputObject->config();
        const QString outputId = WallpaperManager::getOutputId(outputObject);
        const QString primaryOutputId = m_globalConfig->primaryOutputId();
        if (primaryOutputId == outputId) {
            m_rootSurfaceContainer->setPrimaryOutput(outputObject);
        } else if (m_rootSurfaceContainer->primaryOutput()
                   && m_rootSurfaceContainer->primaryOutput()->output()
                   && !m_rootSurfaceContainer->primaryOutput()->output()->isEnabled()
                   && !currentPrimaryMatchesId(m_rootSurfaceContainer, primaryOutputId)) {
            m_rootSurfaceContainer->setPrimaryOutput(outputObject);
        }

        if (!hasSavedOutputState(config)) {
            return;
        }

        const int width = static_cast<int>(config->width());
        const int height = static_cast<int>(config->height());
        const int refresh = static_cast<int>(config->refresh());
        const double scale = config->scale();
        const qlonglong transform = config->transform();
        if (width <= 0 || height <= 0 || refresh <= 0 || scale <= 0.0) {
            qCWarning(lcTlCore) << "Ignoring invalid output dconfig for" << output->name()
                                << "width:" << width
                                << "height:" << height
                                << "refresh:" << refresh
                                << "scale:" << scale;
            return;
        }
        if (transform < WL_OUTPUT_TRANSFORM_NORMAL || transform > WL_OUTPUT_TRANSFORM_FLIPPED_270) {
            qCWarning(lcTlCore) << "Ignoring invalid output dconfig for" << output->name()
                                << "transform:" << transform;
            return;
        }

        qw_output_state newState;
        newState.set_enabled(true);

        if (auto *layout = m_rootSurfaceContainer->outputLayout()) {
            layout->move(output, QPoint(static_cast<int>(config->x()), static_cast<int>(config->y())));
        }

        wlr_output_mode *mode, *configMode = nullptr;
        wl_list_for_each(mode, &output->nativeHandle()->modes, link) {
            if (mode->width == width && mode->height == height && mode->refresh == refresh) {
                configMode = mode;
                break;
            }
        }
        if (configMode)
            newState.set_mode(configMode);
        else
            newState.set_custom_mode(width,
                                     height,
                                     refresh);

        newState.set_adaptive_sync_enabled(config->adaptiveSyncEnabled());
        newState.set_transform(static_cast<wl_output_transform>(transform));
        newState.set_scale(scale);
        if (!output->handle()->commit_state(newState)) {
            qCCritical(lcTlCore) << "commit failed on output" << output->name();
            return;
        }

        if (auto *outputItem = outputObject->outputItem()) {
            QMetaObject::invokeMethod(outputItem,
                                      "setTransform",
                                      Q_ARG(QVariant, QVariant::fromValue(static_cast<WOutput::Transform>(transform))));
        }
    };
    auto *outputConfig = o->config();
    if (outputConfig->isInitializeFailed()) {
        publishOutput();
    } else {
        if (!outputConfig->isInitializeSucceeded()) {
            connect(outputConfig, &OutputConfig::configInitializeFailed, o, publishOutput);
        }
        runWhenOutputConfigInitialized(outputConfig,
                                       o,
                                       [this,
                                        restoreOutputConfig = std::move(restoreOutputConfig),
                                        outputObject = QPointer<Output>(o)]() mutable {
                                           runWhenTreelandConfigInitialized(m_globalConfig.get(),
                                                                           outputObject,
                                                                           std::move(restoreOutputConfig));
                                       });
    }
}

void Helper::onOutputRemoved(WOutput *output)
{
    auto index = indexOfOutput(output);
    Q_ASSERT(index >= 0);
    const auto o = m_outputList.takeAt(index);

    const auto &surfaces = getWorkspaceSurfaces(o);
    if (m_mode == OutputMode::Copy) {
        if (m_outputConfigState) {
            m_outputConfigState->recordCopyModeExit();
        }

        m_mode = OutputMode::Extension;
        Q_EMIT outputModeChanged();

        QList<Output *> outputsToConvert;
        QList<Output *> oldOutputsToDelete;

        bool removedWasPrimary = (output == m_rootSurfaceContainer->primaryOutput()->output());
        Output *primaryCandidate = nullptr;

        for (int i = 0; i < m_outputList.size(); i++) {
            Output *copyOutput = m_outputList.at(i);

            if (copyOutput->isPrimary()) {
                if (!primaryCandidate)
                    primaryCandidate = copyOutput;
                continue;
            }

            Output *normalOutput = createNormalOutput(copyOutput->output());
            normalOutput->enable();

            outputsToConvert.append(normalOutput);
            oldOutputsToDelete.append(copyOutput);

            m_outputList.replace(i, normalOutput);

            if (!primaryCandidate) {
                primaryCandidate = normalOutput;
            }
        }

        if (removedWasPrimary && primaryCandidate) {
            m_rootSurfaceContainer->setPrimaryOutput(primaryCandidate);
            if (!surfaces.isEmpty()) {
                moveSurfacesToOutput(surfaces, primaryCandidate, o);
            }
        }

        if (removedWasPrimary) {
            m_rootSurfaceContainer->removeOutput(o);
        }

        for (auto oldOutput : std::as_const(oldOutputsToDelete)) {
            m_rootSurfaceContainer->removeOutput(oldOutput);
            delete oldOutput;
        }

    } else {
        bool wasPrimary = (o == m_rootSurfaceContainer->primaryOutput());

        if (wasPrimary && m_outputConfigState) {
            m_outputConfigState->markScreenAsPrimary(o->output()->name());
        }

        m_rootSurfaceContainer->removeOutput(o);
        if (m_outputLifecycleManager) {
            m_outputLifecycleManager->setMode(OutputLifecycleManager::Mode::Extension);
            m_outputLifecycleManager->onScreenRemoved(o, surfaces);
        }
    }

    m_outputManager->removeOutput(output);
    m_wallpaperManager->removeOutputWallpaper(output->handle()->handle());

    m_powerOffOutputs.remove(output->nativeHandle());

    delete o;
}

void Helper::onSurfaceModeChanged(WSurface *surface, WXdgDecorationManager::DecorationMode mode)
{
    auto s = m_rootSurfaceContainer->getSurface(surface);
    if (!s)
        return;
    s->setNoDecoration(mode != WXdgDecorationManager::Server);
}

void Helper::setGamma(struct wlr_gamma_control_manager_v1_set_gamma_event *event)
{
    auto *qwOutput = qw_output::from(event->output);
    size_t ramp_size = 0;
    uint16_t *r = nullptr, *g = nullptr, *b = nullptr;
    wlr_gamma_control_v1 *gamma_control = event->control;
    if (gamma_control) {
        ramp_size = gamma_control->ramp_size;
        r = gamma_control->table;
        g = gamma_control->table + gamma_control->ramp_size;
        b = gamma_control->table + 2 * gamma_control->ramp_size;
    }
    qw_output_state newState;
    newState.set_gamma_lut(ramp_size, r, g, b);
    if (!qwOutput->commit_state(newState)) {
        qCCritical(lcTlCore, "commit failed on output  %s", qwOutput->handle()->name);
        qCWarning(lcTlCore) << "Failed to set gamma lut!";
        // TODO: use software impl it.
        qw_gamma_control_v1::from(gamma_control)->send_failed_and_destroy();
    }
}

void Helper::handleCopyModeOutputDisable(Output *affectedOutput)
{
    int affectedIndex = m_outputList.indexOf(affectedOutput);
    if (affectedIndex < 0) {
        qCWarning(lcTlCore) << "Disabled output not found in m_outputList";
        return;
    }

    if (m_outputConfigState) {
        m_outputConfigState->recordCopyModeExit();
    }

    m_mode = OutputMode::Extension;
    Q_EMIT outputModeChanged();

    // Convert CopyOutputs to Normal outputs (independent displays)
    // Keep the disabled output in the list - it will receive disable state through normal wlroots flow
    Output *primaryCandidate = nullptr;
    const auto &surfaces = getWorkspaceSurfaces(affectedOutput);
    for (int i = 0; i < m_outputList.size(); i++) {
        if (i == affectedIndex) {
            continue;
        }

        Output *copyOutput = m_outputList.at(i);
        Output *normalOutput = createNormalOutput(copyOutput->output());
        normalOutput->enable();
        copyOutput->deleteLater();
        m_outputList.replace(i, normalOutput);

        if (!primaryCandidate) {
            primaryCandidate = normalOutput;
        }
    }

    if (primaryCandidate) {
        if (!surfaces.isEmpty()) {
            moveSurfacesToOutput(surfaces, primaryCandidate, affectedOutput);
        }
        m_rootSurfaceContainer->setPrimaryOutput(primaryCandidate);
    }
}

void Helper::onOutputTestOrApply(qw_output_configuration_v1 *config, bool onlyTest)
{
    QList<WOutputState> states = m_outputManager->stateListPending();

    if (onlyTest) {
        bool ok = true;
        for (const auto &state : std::as_const(states)) {
            WOutputViewport *viewport = getOwnOutputViewport(state.output);
            if (!viewport) {
                ok = false;
                continue;
            }

            WOutputRenderWindow *renderWindow = viewport->outputRenderWindow();
            if (!renderWindow) {
                ok = false;
                continue;
            }
            qw_output_state newState;
            newState.set_enabled(state.enabled);
            if (state.enabled) {
                if (state.mode)
                    newState.set_mode(state.mode);
                else
                    newState.set_custom_mode(state.customModeSize.width(),
                                             state.customModeSize.height(),
                                             state.customModeRefresh);
                newState.set_adaptive_sync_enabled(state.adaptiveSyncEnabled);
                newState.set_transform(static_cast<wl_output_transform>(state.transform));
                newState.set_scale(state.scale);
            }
            ok &= state.output->handle()->test_state(newState);
        }

        m_outputManager->sendResult(config, ok);
        return;
    }

    if (m_pendingOutputConfig.config) {
        m_outputManager->sendResult(m_pendingOutputConfig.config, false);
    }

    // Handle Copy Mode transition when primary output is disabled
    if (m_mode == OutputMode::Copy) {
        for (const auto &state : std::as_const(states)) {
            if (!state.enabled) {
                Output *affectedOutput = getOutput(state.output);
                if (affectedOutput && affectedOutput == m_rootSurfaceContainer->primaryOutput()) {
                    handleCopyModeOutputDisable(affectedOutput);
                    break;
                }
            }
        }
    }

    m_pendingOutputConfig.config = config;
    m_pendingOutputConfig.states = states;
    m_pendingOutputConfig.pendingCommits = 0;
    m_pendingOutputConfig.allSuccess = true;

    if (m_outputLifecycleManager) {
        m_outputLifecycleManager->setMode(m_mode == OutputMode::Extension
                                              ? OutputLifecycleManager::Mode::Extension
                                              : OutputLifecycleManager::Mode::Copy);

        for (const auto &state : std::as_const(states)) {
            Output *outputObj = getOutput(state.output);
            if (!outputObj) {
                continue;
            }

            if (!state.enabled && state.output->isEnabled()) {
                const auto &surfaces = getWorkspaceSurfaces(outputObj);
                m_outputLifecycleManager->onScreenDisabled(outputObj, surfaces);
            } else if (state.enabled && !state.output->isEnabled()) {
                m_outputLifecycleManager->onScreenEnabled(outputObj);
                if (m_outputLifecycleManager->takeCopyModeRestoreIntent()) {
                    restoreCopyMode();
                }
            }
        }
    }

    for (const auto &state : std::as_const(states)) {
        // Skip outputs that have been removed (e.g., disabled in Copy mode)
        Output *output = getOutput(state.output);
        if (!output) {
            continue;
        }

        WOutputViewport *viewport = getOwnOutputViewport(state.output);
        if (!viewport) {
            m_outputManager->sendResult(config, false);
            m_pendingOutputConfig = {};
            return;
        }

        WOutputRenderWindow *renderWindow = viewport->outputRenderWindow();
        if (!renderWindow) {
            qCWarning(lcTlCore) << "No renderWindow for output" << state.output->name();
            m_outputManager->sendResult(config, false);
            m_pendingOutputConfig = {};
            return;
        }

        if (state.enabled) {
            auto *layout = m_rootSurfaceContainer->outputLayout();
            if (layout) {
                layout->move(state.output, QPoint(state.x, state.y));
            }
        }

        auto outputHelper = renderWindow->getOutputHelper(viewport);
        if (!outputHelper) {
            qCWarning(lcTlCore) << "No output helper for viewport" << viewport;
            m_outputManager->sendResult(config, false);
            m_pendingOutputConfig = {};
            return;
        }

        WOutputHelper::ExtraState extraState;
        wlr_output_state_set_enabled(extraState.get(), state.enabled);

        // Only set mode/scale/transform properties when enabling output.
        // wlroots doesn't allow setting these properties on disabled outputs,
        // so they are persisted only after a successful enabled commit.
        if (state.enabled) {
            if (state.mode) {
                wlr_output_state_set_mode(extraState.get(), state.mode);
            } else {
                wlr_output_state_set_custom_mode(extraState.get(),
                                                 state.customModeSize.width(),
                                                 state.customModeSize.height(),
                                                 state.customModeRefresh);
            }

            wlr_output_state_set_scale(extraState.get(), state.scale);
            wlr_output_state_set_transform(extraState.get(),
                                          static_cast<wl_output_transform>(state.transform));
            wlr_output_state_set_adaptive_sync_enabled(extraState.get(), state.adaptiveSyncEnabled);

            if (auto outputItem = qobject_cast<WOutputItem*>(viewport->parentItem())) {
                QMetaObject::invokeMethod(outputItem, "setTransform",
                    Q_ARG(QVariant, QVariant::fromValue(static_cast<WOutput::Transform>(state.transform))));
            }
        }

        if (!outputHelper->setExtraState(extraState)) {
            qCWarning(lcTlCore) << "Failed to set extra state for output" << state.output->name();
            m_outputManager->sendResult(config, false);
            m_pendingOutputConfig = {};
            return;
        }
        auto config = m_pendingOutputConfig.config;
        QPointer<Helper> self(this);
        outputHelper->scheduleCommitJob(
            [self, config, extraState, renderWindow, viewport](bool success, WOutputHelper::ExtraState committedState) {
                if (!self) {
                    return;
                }

                if (committedState == extraState) {
                    self->onOutputCommitFinished(config, success);
                    if (success && committedState) {
                        bool wasStateOnlyCommit = (committedState->committed & (WLR_OUTPUT_STATE_MODE |
                                                                                WLR_OUTPUT_STATE_SCALE |
                                                                                WLR_OUTPUT_STATE_TRANSFORM |
                                                                                WLR_OUTPUT_STATE_ENABLED)) &&
                                                  !(committedState->committed & WLR_OUTPUT_STATE_BUFFER);
                        bool isDisable = (committedState->committed & WLR_OUTPUT_STATE_ENABLED) && !committedState->enabled;
                        if (wasStateOnlyCommit && !isDisable) {
                            renderWindow->update(viewport);
                        }
                    }
                } else {
                    qCWarning(lcTlCore) << "Commit callback received unexpected state pointer!"
                                            << "Expected:" << extraState.get()
                                            << "Got:" << committedState.get();
                    self->onOutputCommitFinished(config, false);
                }
            },
            WOutputHelper::AfterCommitStage
        );
        m_pendingOutputConfig.pendingCommits++;
        renderWindow->update(viewport);

        // Special handling for disabled → enabled transition
        // wlroots doesn't send frame events for disabled outputs,
        // so we need to force render to trigger the commit
        if (state.enabled && !state.output->isEnabled()) {
            renderWindow->render(viewport, true);
        }
    }
}

void Helper::onOutputCommitFinished(qw_output_configuration_v1 *config, bool success)
{
    if (!config) {
        return;
    }

    if (config != m_pendingOutputConfig.config) {
        return;
    }

    if (!success) {
        m_pendingOutputConfig.allSuccess = false;
    }

    m_pendingOutputConfig.pendingCommits--;
    if (m_pendingOutputConfig.pendingCommits == 0) {
        bool ok = m_pendingOutputConfig.allSuccess;
        if (ok) {
            for (const WOutputState &state : std::as_const(m_pendingOutputConfig.states)) {
                auto *output = getOutput(state.output);
                if (!output) {
                    qCWarning(lcTlCore) << "Cannot save output dconfig, output object not found:"
                                        << state.output->name();
                    continue;
                }

                auto *outputConfig = output->config();
                const bool enabled = state.enabled;
                const qlonglong x = state.x;
                const qlonglong y = state.y;
                const qlonglong width = state.mode ? state.mode->width : state.customModeSize.width();
                const qlonglong height = state.mode ? state.mode->height : state.customModeSize.height();
                const qlonglong refresh = state.mode ? state.mode->refresh : state.customModeRefresh;
                const qlonglong transform = output->output()->nativeHandle()->transform;
                const double scale = state.scale;
                const bool adaptiveSyncEnabled = state.adaptiveSyncEnabled;
                runWhenOutputConfigInitialized(outputConfig, output, [outputConfig = QPointer<OutputConfig>(outputConfig),
                                                                      enabled,
                                                                      x,
                                                                      y,
                                                                      width,
                                                                      height,
                                                                      refresh,
                                                                      transform,
                                                                      scale,
                                                                      adaptiveSyncEnabled] {
                    if (!outputConfig) {
                        return;
                    }

                    outputConfig->setEnabled(enabled);
                    if (!enabled) {
                        return;
                    }

                    outputConfig->setX(x);
                    outputConfig->setY(y);
                    outputConfig->setWidth(width);
                    outputConfig->setHeight(height);
                    outputConfig->setRefresh(refresh);
                    outputConfig->setTransform(transform);
                    outputConfig->setScale(scale);
                    outputConfig->setAdaptiveSyncEnabled(adaptiveSyncEnabled);
                });
            }
        }
        m_outputManager->sendResult(config, ok);
        m_pendingOutputConfig = {};
    }
}

void Helper::onSetOutputPowerMode(wlr_output_power_v1_set_mode_event *event)
{
    auto output = qw_output::from(event->output);
    qw_output_state newState;

    switch (event->mode) {
    case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
        if (m_powerOffOutputs.contains(event->output))
            return; // already disabled by output_power
        if (!output->handle()->enabled)
            return; // already disabled by output_management, not ours
        newState.set_enabled(false);
        if (!output->commit_state(newState)) {
            qCCritical(lcTlCore, "commit failed on output %s", output->handle()->name);
            return;
        }
        m_powerOffOutputs.insert(event->output);
        break;
    case ZWLR_OUTPUT_POWER_V1_MODE_ON:
        if (!m_powerOffOutputs.remove(event->output))
            return; // not disabled by output_power, nothing to do
        newState.set_enabled(true);
        if (!output->commit_state(newState)) {
            qCCritical(lcTlCore, "commit failed on output %s", output->handle()->name);
            m_powerOffOutputs.insert(event->output);
            return;
        }
        break;
    }
}

void Helper::onNewIdleInhibitor(wlr_idle_inhibitor_v1 *wlr_inhibitor)
{
    if (!wlr_inhibitor->surface) {
        qCInfo(lcTlCore) << "Ignoring idle inhibitor with null surface";
        return;
    }

    auto wsurface = WSurface::fromHandle(wlr_inhibitor->surface);
    if (!wsurface) {
        qCWarning(lcTlCore) << "No WSurface found for idle inhibitor surface"
                                << wlr_inhibitor->surface;
        return;
    }

    auto inhibitor = qw_idle_inhibitor_v1::from(wlr_inhibitor);
    m_idleInhibitors.append(inhibitor);

    connect(inhibitor, &qw_idle_inhibitor_v1::before_destroy, this, [this, inhibitor]() {
        m_idleInhibitors.removeOne(inhibitor);
        updateIdleInhibitor();
    });

    connect(wsurface, &WSurface::mappedChanged, inhibitor, [this]() {
        updateIdleInhibitor();
    });

    auto toplevel = WXdgToplevelSurface::fromSurface(wsurface);
    if (toplevel) {
        connect(toplevel, &WXdgToplevelSurface::minimizeChanged, inhibitor, [this]() {
            updateIdleInhibitor();
        });
    }

    updateIdleInhibitor();
}

void Helper::updateIdleInhibitor()
{
    if (m_screensaverInterfaceV1->isInhibited()) {
        m_idleNotifier->set_inhibited(true);
        return;
    }
    for (const auto &inhibitor : std::as_const(m_idleInhibitors)) {
        auto wsurface = WSurface::fromHandle((*inhibitor)->surface);
        if (!wsurface)
            continue;
        bool visible = wsurface->mapped();
        auto toplevel = WXdgToplevelSurface::fromSurface(wsurface);
        if (toplevel)
            visible &= !toplevel->isMinimized();

        if (visible) {
            m_idleNotifier->set_inhibited(true);
            return;
        }
    }
    m_idleNotifier->set_inhibited(false);
}

void Helper::onShowDesktop()
{
    WindowManagementInterfaceV1::DesktopState s = m_windowManagementInterfaceV1->desktopState();
    if (m_showDesktop == s
        || (s != WindowManagementInterfaceV1::DesktopState::Normal
            && s != WindowManagementInterfaceV1::DesktopState::Show))
        return;

    m_showDesktop = s;
    const auto &surfaces = getWorkspaceSurfaces();
    for (auto &surface : surfaces) {
        if (surface->isMinimized()) {
            continue;
        }
        if (s == WindowManagementInterfaceV1::DesktopState::Normal) {
            surface->startShowDesktopAnimation(true);
        } else if (s == WindowManagementInterfaceV1::DesktopState::Show) {
            surface->startShowDesktopAnimation(false);
        }
    }
}

void Helper::onSetCopyOutput(VirtualOutputInterfaceV1 *interface)
{
    Output *mirrorOutput = nullptr;
    for (Output *output : std::as_const(m_outputList)) {
        if (!interface->outputList().contains(output->output()->name())) {
            QString screen = output->output()->name() + " does not exist!";
            interface->sendError(VirtualOutputInterfaceV1::INVALID_OUTPUT, screen);

            return;
        }

        if (!output->isPrimary()) {
            QString screen =
                output->output()->name() + " is already a copy screen, invalid setting!";
            interface->sendError(VirtualOutputInterfaceV1::INVALID_OUTPUT, screen);
            return;
        }

        if (output->output()->name() == interface->outputList().at(0))
            mirrorOutput = output;
    }

    for (int i = 0; i < m_outputList.size(); i++) {
        Output *currentOutput = m_outputList.at(i);
        if (currentOutput == mirrorOutput)
            continue;

        // When setting the primaryOutput as a copy screen, set the mirrorOutput
        // as the home screen.
        if (m_rootSurfaceContainer->primaryOutput() == currentOutput)
            m_rootSurfaceContainer->setPrimaryOutput(mirrorOutput);

        Output *o = createCopyOutput(currentOutput->output(), mirrorOutput);
        m_rootSurfaceContainer->removeOutput(currentOutput);
        currentOutput->deleteLater();
        m_outputList.replace(i, o);
    }

    m_mode = OutputMode::Copy;
    const auto &surfaces = getWorkspaceSurfaces();
    moveSurfacesToOutput(surfaces, mirrorOutput, nullptr);
}

void Helper::onRestoreCopyOutput(VirtualOutputInterfaceV1 *interface)
{
    const QString targetName = interface->outputList().at(0);
    if (!std::any_of(m_outputList.constBegin(), m_outputList.constEnd(),
                     [&targetName](const Output *output) { return output->output()->name() == targetName; })) {
        interface->sendError(VirtualOutputInterfaceV1::INVALID_OUTPUT,
            QString("Target output %1 does not exist!").arg(targetName));

        return;
    }

    for (int i = 0; i < m_outputList.size(); i++) {
        Output *currentOutput = m_outputList.at(i);
        if (currentOutput->output()->name() == targetName)
            continue;

        Output *o = createNormalOutput(m_outputList.at(i)->output());
        o->enable();
        m_outputList.at(i)->deleteLater();
        m_outputList.replace(i, o);
    }
    m_mode = OutputMode::Extension;
}

void Helper::onSurfaceWrapperAdded(SurfaceWrapper *wrapper)
{
    if (wrapper->isIMCandidatePanel())
        return;

    bool isXdgToplevel = wrapper->type() == SurfaceWrapper::Type::XdgToplevel;
    bool isXdgPopup = wrapper->type() == SurfaceWrapper::Type::XdgPopup;
    bool isXwayland = wrapper->type() == SurfaceWrapper::Type::XWayland;
    bool isLayer = wrapper->type() == SurfaceWrapper::Type::Layer;

    connect(m_sessionManager->activeSession().lock().get(),
            &Session::aboutToBeDestroyed,
            wrapper,
            &SurfaceWrapper::closeSurface);

    if (isXdgToplevel || isXdgPopup || isLayer) {
        auto *attached =
            new Personalization(wrapper->shellSurface(), m_personalizationInterfaceV1, wrapper);
        connect(wrapper, &SurfaceWrapper::aboutToBeInvalidated,
                attached, &Personalization::deleteLater);

        auto updateNoTitlebar = [this, attached] {
            auto wrapper = attached->surfaceWrapper();
            if (attached->noTitlebar()) {
                wrapper->setNoTitleBar(true);
                auto layer = qobject_cast<WLayerSurface *>(wrapper->shellSurface());
                if (!isLaunchpad(layer)) {
                    wrapper->setNoDecoration(false);
                }
            } else {
                wrapper->setNoTitleBar(false);
                wrapper->setNoDecoration(m_xdgDecorationManager->modeBySurface(wrapper->surface())
                                     != WXdgDecorationManager::Server);
            }
        };

        if (isXdgToplevel) {
            connect(
                m_xdgDecorationManager,
                &WXdgDecorationManager::surfaceModeChanged,
                attached,
                [attached, updateNoTitlebar](
                    WAYLIB_SERVER_NAMESPACE::WSurface *surface,
                    [[maybe_unused]] Waylib::Server::WXdgDecorationManager::DecorationMode mode) {
                    if (surface == attached->surfaceWrapper()->surface()) {
                        updateNoTitlebar();
                    }
                });
        }

        connect(attached, &Personalization::windowStateChanged, this, updateNoTitlebar);
        updateNoTitlebar();

        auto updateBlur = [attached] {
            attached->surfaceWrapper()->setBlur(attached->backgroundType() == Personalization::BackgroundType::Blur);
        };
        connect(attached, &Personalization::backgroundTypeChanged, this, updateBlur);
        auto updateCornerRadius = [attached] {
            attached->surfaceWrapper()->setRadius(attached->cornerRadius());
        };
        connect(attached, &Personalization::cornerRadiusChanged, this, updateCornerRadius);
        updateCornerRadius();
        updateBlur();
        if (isLayer) {
            auto layer = qobject_cast<WLayerSurface *>(wrapper->shellSurface());
            if (isLaunchpad(layer))
                wrapper->setCoverEnabled(true);
        }
    }

    if (isXwayland) {
        auto xwaylandSurface = qobject_cast<WXWaylandSurface *>(wrapper->shellSurface());

        uint32_t windowId = 0;
        if (xwaylandSurface && xwaylandSurface->handle() && xwaylandSurface->handle()->handle()) {
            windowId = xwaylandSurface->handle()->handle()->window_id;
        }
        bool isBypass = xwaylandSurface ? xwaylandSurface->isBypassManager() : false;
        qCInfo(lcTlXwayland) << "[WRAPPER_ADDED] XWayland wrapper added to helper:"
                              << "window_id=" << windowId
                              << "wrapper=" << wrapper
                              << "appId=" << wrapper->appId()
                              << "isBypassManager=" << isBypass;

        auto updateDecorationTitleBar = [wrapper, xwaylandSurface, sessionManager = m_sessionManager]() {
            auto *xwayland = xwaylandSurface->xwayland();
            xcb_connection_t *connection = xwayland ? xwayland->xcbConnection() : nullptr;
            xcb_atom_t atom;
            if (xwayland) {
                if (auto session = sessionManager->sessionForXWayland(xwayland))
                    atom = session->noTitlebarAtom();
                else
                    atom = XCB_ATOM_NONE;
            } else {
                atom = XCB_ATOM_NONE;
            }
            if (!xwaylandSurface->isBypassManager()) {
                if (atom && connection
                    && !readWindowProperty(connection,
                                           xwaylandSurface->handle()->handle()->window_id,
                                           atom,
                                           XCB_ATOM_CARDINAL)
                            .isEmpty()) {
                    wrapper->setNoTitleBar(true);
                } else {
                    wrapper->setNoTitleBar(xwaylandSurface->decorationsFlags()
                                           & WXWaylandSurface::DecorationsNoTitle);
                }
                wrapper->setNoDecoration(xwaylandSurface->decorationsFlags()
                                         & WXWaylandSurface::DecorationsNoBorder);
            } else {
                wrapper->setNoTitleBar(true);
                wrapper->setNoDecoration(true);
            }
        };
        // When x11 surface dissociate, SurfaceWrapper will be destroyed immediately
        // but WXWaylandSurface will not, so must connect to `wrapper`
        xwaylandSurface->safeConnect(&WXWaylandSurface::bypassManagerChanged,
                                     wrapper,
                                     updateDecorationTitleBar);
        xwaylandSurface->safeConnect(&WXWaylandSurface::decorationsFlagsChanged,
                                     wrapper,
                                     updateDecorationTitleBar);
        updateDecorationTitleBar();

        wrapper->setHideByWorkspace(!surfaceBelongsToCurrentSession(wrapper));
    }

    if (!isLayer) {
        [[maybe_unused]] auto windowOverlapChecker = new WindowOverlapChecker(wrapper, wrapper);
    }

#ifndef DISABLE_DDM
    if (isLayer) {
        connect(this, &Helper::currentModeChanged, wrapper, [this, wrapper] {
            wrapper->setHideByLockScreen(m_currentMode == CurrentMode::LockScreen);
        });
        wrapper->setHideByLockScreen(m_currentMode == CurrentMode::LockScreen);
    }
#endif

    if (!wrapper->skipDockPreView()) {
        m_foreignToplevel->addSurface(wrapper->shellSurface());
        m_extForeignToplevelListV1->addSurface(wrapper->shellSurface());
    }
    connect(wrapper, &SurfaceWrapper::skipDockPreViewChanged, this, [this, wrapper] {
        if (wrapper->skipDockPreView()) {
            m_foreignToplevel->removeSurface(wrapper->shellSurface());
            m_extForeignToplevelListV1->removeSurface(wrapper->shellSurface());
        } else {
            m_foreignToplevel->addSurface(wrapper->shellSurface());
            m_extForeignToplevelListV1->addSurface(wrapper->shellSurface());
        }
    });
}

void Helper::onSurfaceWrapperAboutToRemove(SurfaceWrapper *wrapper)
{
    if (wrapper->isIMCandidatePanel())
        return;

    if (!wrapper->skipDockPreView()) {
        m_foreignToplevel->removeSurface(wrapper->shellSurface());
        m_extForeignToplevelListV1->removeSurface(wrapper->shellSurface());
    }
    // Ensure the wrapper is removed from active history early to avoid cascading on half-invalid entries
    if (wrapper && wrapper->workspaceId() != -1) {
        auto ws = workspace();
        if (ws) {
            Q_ASSERT(ws == wrapper->container());
            ws->removeActivedSurface(wrapper);
        }
    }
}

bool Helper::surfaceBelongsToCurrentSession(SurfaceWrapper *wrapper)
{
    if (wrapper->type() == SurfaceWrapper::Type::SplashScreen) {
        // TODO(rewine): Determine which user the splash screen belongs to by invoking the client of the prelaunch-splash protocol.
        // Currently, treeland does not support logging in with multiple users at the same time
        // so it is temporarily assumed that the splash screen must belong to the current user.
        return true;
    }
    WClient *client = wrapper->surface()->waylandClient();
    WSocket *socket = client ? client->socket()->rootSocket() : nullptr;
    return socket && socket->isEnabled();
}

void Helper::deleteTaskSwitch()
{
    if (m_taskSwitch) {
        m_taskSwitch->deleteLater();
        m_taskSwitch = nullptr;
    }
    if (m_currentMode == CurrentMode::WindowSwitch) {
        setCurrentMode(CurrentMode::Normal);
    }
}

void Helper::init(Treeland::Treeland *treeland)
{
    m_treeland = treeland;
    connect(m_sessionManager, &SessionManager::sessionChanged, treeland, &Treeland::Treeland::SessionChanged);

    auto engine = qmlEngine();
    m_greeterProxy = engine->singletonInstance<GreeterProxy *>("Treeland", "GreeterProxy");
    m_userModel = engine->singletonInstance<UserModel *>("Treeland", "UserModel");
    m_sessionModel = engine->singletonInstance<SessionModel *>("Treeland", "SessionModel");

    engine->setContextForObject(m_renderWindow, engine->rootContext());
    engine->setContextForObject(m_renderWindow->contentItem(), engine->rootContext());
    m_rootSurfaceContainer->setQmlEngine(engine);
    m_rootSurfaceContainer->init(m_server);

    m_backend = m_server->attach<WBackend>();
    m_seatManager = new SeatsManager(m_server, this);

    m_ddmInterfaceV1 = m_server->attach<DDMInterfaceV1>();

    m_outputManager = m_server->attach<WOutputManagerV1>();
    connect(m_backend, &WBackend::outputAdded, this, &Helper::onOutputAdded);
    connect(m_backend, &WBackend::outputRemoved, this, &Helper::onOutputRemoved);

    m_ddeShellV1 = m_server->attach<DDEShellManagerInterfaceV1>();
    connect(m_ddeShellV1, &DDEShellManagerInterfaceV1::toggleMultitaskview, this, [this] {
        if (m_multitaskView) {
            m_multitaskView->toggleMultitaskView(IMultitaskView::ActiveReason::ShortcutKey);
        }
    });
    connect(m_ddeShellV1,
            &DDEShellManagerInterfaceV1::requestPickWindow,
            this,
            &Helper::handleWindowPicker);
    connect(m_ddeShellV1,
            &DDEShellManagerInterfaceV1::lockScreenCreated,
            this,
            &Helper::handleLockScreen);
    m_shellHandler->createComponent(engine, m_renderWindow->contentItem());

    m_foreignToplevel = m_server->attach<WForeignToplevel>();
    m_extForeignToplevelListV1 = m_server->attach<WExtForeignToplevelListV1>();

    connect(m_shellHandler,
            &ShellHandler::surfaceWrapperAdded,
            this,
            &Helper::onSurfaceWrapperAdded);

    connect(m_shellHandler,
            &ShellHandler::surfaceWrapperAboutToRemove,
            this,
            &Helper::onSurfaceWrapperAboutToRemove);

    auto *xdgOutputManager =
        m_server->attach<WXdgOutputManager>(m_rootSurfaceContainer->outputLayout());

    m_outputManagerV1 = m_server->attach<OutputManagerV1>();
    connect(m_rootSurfaceContainer,
            &RootSurfaceContainer::primaryOutputChanged,
            m_outputManagerV1,
            &OutputManagerV1::onPrimaryOutputChanged);
    connect(m_rootSurfaceContainer,
            &RootSurfaceContainer::primaryOutputChanged,
            m_sessionManager,
            &SessionManager::syncActiveSessionXWaylandPrimaryOutput);
    m_wallpaperColorV1 = m_server->attach<WallpaperColorInterfaceV1>();
    m_windowManagementInterfaceV1 = m_server->attach<WindowManagementInterfaceV1>();
    m_virtualOutputInterfaceV1 = m_server->attach<VirtualOutputManagerInterfaceV1>();

    auto captureManagerV1 = m_server->attach<CaptureManagerV1>();
    captureManagerV1->setOutputRenderWindow(m_renderWindow);

    connect(
        captureManagerV1,
        &CaptureManagerV1::contextInSelectionChanged,
        this,
        [this, captureManagerV1] {
            if (captureManagerV1->contextInSelection()) {
                m_captureSelector = qobject_cast<CaptureSourceSelector *>(
                    qmlEngine()->createCaptureSelector(m_rootSurfaceContainer, captureManagerV1));
            } else if (m_captureSelector) {
                m_captureSelector->deleteLater();
            }
        });
    m_personalizationInterfaceV1 = m_server->attach<PersonalizationManagerInterfaceV1>();

    auto updateCurrentUser = [this] {
        m_config.reset(TreelandUserConfig::createByName("org.deepin.dde.treeland.user",
                                                  "org.deepin.dde.treeland",
                                                  "/" + m_userModel->currentUserName()));
        // Notify QML that the config pointer has changed so bindings (e.g. sourceSize
        // on WQuickCursor) reconnect their notifiers to the new TreelandUserConfig object.
        Q_EMIT configChanged();
        connect(m_config.get(),
                &TreelandUserConfig::cursorThemeNameChanged,
                m_sessionManager,
                &SessionManager::syncActiveSessionCursorSettings);
        connect(m_config.get(),
                &TreelandUserConfig::cursorSizeChanged,
                m_sessionManager,
                &SessionManager::syncActiveSessionCursorSettings);
        auto user = m_userModel->currentUser();
        m_personalizationInterfaceV1->setUserId(user ? user->UID() : getuid());
        // TODO(YaoBing Xiao): remove "dde"
        if (m_userModel->currentUserName() == "dde") {
            return;
        }

        m_inputManager->setupSeatUserConfig(m_userModel->currentUserName());
        auto onConfigInitialized = [this] {
            m_sessionManager->syncActiveSessionCursorSettings();
            syncPaletteTypeWithWindowThemeType(m_config->windowThemeType());
            m_wallpaperManager->updateWallpaperConfig();
            tryInitRemoteSource();
            //TODO: Isolate workspaces for different users to prevent them from sharing the same one.
            if (m_userModel->currentUserName() != "dde")
                m_shellHandler->workspace()->reloadFromConfig();
        };
        // TODO(YaoBing Xiao): pre-initialize dconfig, remove isInitializeSucceeded
#if TREELANDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
        if (m_config->isInitializeSucceeded()) {
#else
        if (m_config->isInitializeSucceed()) {
#endif
            onConfigInitialized();
        } else {
            connect(m_config.get(),
                    &TreelandUserConfig::configInitializeSucceed,
                    this,
                    onConfigInitialized);
        }
    };
    connect(m_userModel, &UserModel::currentUserNameChanged, this, updateCurrentUser);

    updateCurrentUser();

    connect(m_windowManagementInterfaceV1,
            &WindowManagementInterfaceV1::desktopStateChanged,
            this,
            &Helper::onShowDesktop);

    connect(m_virtualOutputInterfaceV1,
            &VirtualOutputManagerInterfaceV1::requestCreateVirtualOutput,
            this,
            &Helper::onSetCopyOutput);

    connect(m_virtualOutputInterfaceV1,
            &VirtualOutputManagerInterfaceV1::destroyVirtualOutput,
            this,
            &Helper::onRestoreCopyOutput);

    connect(m_rootSurfaceContainer, &RootSurfaceContainer::primaryOutputChanged, this, [this]() {
        if (m_rootSurfaceContainer->primaryOutput()) {
            if (m_lockScreen) {
                m_lockScreen->setPrimaryOutputName(m_rootSurfaceContainer->primaryOutput()->output()->name());
            }
        }
    });

    qmlRegisterUncreatableType<Personalization>("Treeland.Protocols",
                                                1,
                                                0,
                                                "Personalization",
                                                "Only for Enum");

    qmlRegisterUncreatableType<DDEShellHelper>("Treeland.Protocols",
                                               1,
                                               0,
                                               "DDEShellHelper",
                                               "Only for attached");
    qmlRegisterUncreatableType<CaptureSource>("Treeland.Protocols",
                                              1,
                                              0,
                                              "CaptureSource",
                                              "An abstract class");
    qmlRegisterType<CaptureContextV1>("Treeland.Protocols", 1, 0, "CaptureContextV1");
    qmlRegisterType<CaptureSourceSelector>("Treeland.Protocols", 1, 0, "CaptureSourceSelector");

    m_server->attach<WSecurityContextManager>();

    m_server->start();

    // Initialize seats from configuration
    m_primarySeat =
        m_seatManager->initializeFromConfig("/etc/deepin/treeland/seats.json", m_server);
    if (!m_primarySeat) {
        qCCritical(lcTlCore) << "Failed to initialize seats!";
        return;
    }

    // Setup all seats (cursor, keyboard focus, event filter)
    m_seatManager->setupAllSeats(m_renderWindow,
                                 m_rootSurfaceContainer->outputLayout(),
                                 this,
                                 m_rootSurfaceContainer->cursor());

    // Connect device signals and handle device lifecycle
    m_seatManager->connectBackendSignals(m_backend);
    connect(m_seatManager, &SeatsManager::deviceAdded, this, [this](WInputDevice *device) {
        m_seatManager->assignDevice(device,
                                    m_renderWindow,
                                    m_rootSurfaceContainer->outputLayout(),
                                    m_primarySeat);
    });

    // Setup drag request handling for all seats
    const auto seats = m_seatManager->seats();
    for (auto *seat : seats) {
        disconnect(seat, &WSeat::requestDrag, this, nullptr);
        connect(seat, &WSeat::requestDrag, this, [this, seat](WSurface *surface) {
            handleRequestDragForSeat(seat, surface);
        });
    }

    // Assign existing devices
    m_seatManager->assignExistingDevices(m_backend);

    if (m_rootSurfaceContainer) {
        m_rootSurfaceContainer->setupSeatManagement();
    }

    if (!m_primarySeat) {
        qCCritical(lcTlCore) << "No seat available after initialization, cannot continue";
        return;
    }
    m_shellHandler->init(m_server, m_primarySeat);

    connect(m_shellHandler->wallpaperShell(),
            &TreelandWallpaperShellInterfaceV1::wallpaperSurfaceAdded,
            m_wallpaperManager,
            &WallpaperManager::handleWallpaperSurfaceAdded);

    m_renderer = WRenderHelper::createRenderer(m_backend->handle());
    if (!m_renderer) {
        qCFatal(lcTlCore) << "Failed to create renderer";
    }

    m_allocator = qw_allocator::autocreate(*m_backend->handle(), *m_renderer);
    m_renderer->init_wl_display(*m_server->handle());
    qw_drm::create(*m_server->handle(), *m_renderer);

    // free follow display
    m_compositor = qw_compositor::create(*m_server->handle(), 6, *m_renderer);
    qw_subcompositor::create(*m_server->handle());
    qw_screencopy_manager_v1::create(*m_server->handle());
    qw_ext_image_copy_capture_manager_v1::create(*m_server->handle(), 1);
    qw_ext_output_image_capture_source_manager_v1::create(*m_server->handle(), 1);
    m_foreignToplevelImageCaptureManager = qw_ext_foreign_toplevel_image_capture_source_manager_v1::create(*m_server->handle(), 1);
    connect(m_foreignToplevelImageCaptureManager,
            &qw_ext_foreign_toplevel_image_capture_source_manager_v1::notify_new_request,
            this, &Helper::handleNewForeignToplevelCaptureRequest);

    qw_viewporter::create(*m_server->handle());
    m_renderWindow->init(m_renderer, m_allocator);

    auto *xwaylandOutputManager =
        m_server->attach<WXdgOutputManager>(m_rootSurfaceContainer->outputLayout());
    xwaylandOutputManager->setScaleOverride(1.0);

    static const auto isXWaylandClient =
        [sessionManager = QPointer(m_sessionManager)](WClient *client) {
            if (sessionManager) {
                for (const auto &session : std::as_const(sessionManager->sessions())) {
                    if (session && session->xwayland() && session->xwayland()->waylandClient() == client)
                        return true;
                }
            }
        return false;
       };
    xdgOutputManager->setFilter([](WClient *client) { return !isXWaylandClient(client); });
    xwaylandOutputManager->setFilter([](WClient *client) { return isXWaylandClient(client); });
    // User dde does not has a real Logind session, so just pass 0 as id
    m_sessionManager->updateActiveUserSession(QStringLiteral("dde"), 0);
    connect(m_userModel, &UserModel::userLoggedIn, m_sessionManager, &SessionManager::updateActiveUserSession);
    m_xdgDecorationManager = m_server->attach<WXdgDecorationManager>();
    connect(m_xdgDecorationManager,
            &WXdgDecorationManager::surfaceModeChanged,
            this,
            &Helper::onSurfaceModeChanged);

    m_xdgDialogManagerV1 = m_server->attach<WXdgDialogManagerV1>();
    connect(m_xdgDialogManagerV1,
            &WXdgDialogManagerV1::surfaceModalChanged,
            this,
            [this](WXdgToplevelSurface *toplevel, bool modal) {
                if (auto *wrapper = m_rootSurfaceContainer->getSurface(toplevel)) {
                    wrapper->setModal(modal);
                } else {
                    qCWarning(lcTlShell) << "xdg-dialog-v1: no wrapper for toplevel" << toplevel;
                }
            });

    m_xdgToplevelTagManagerV1 = m_server->attach<WXdgToplevelTagManagerV1>();

    auto gammaControlManager = qw_gamma_control_manager_v1::create(*m_server->handle());
    connect(gammaControlManager,
            &qw_gamma_control_manager_v1::notify_set_gamma,
            this,
            &Helper::setGamma);

    connect(m_outputManager,
            &WOutputManagerV1::requestTestOrApply,
            this,
            &Helper::onOutputTestOrApply);

    m_server->attach<WCursorShapeManagerV1>();
    qw_fractional_scale_manager_v1::create(*m_server->handle(), WLR_FRACTIONAL_SCALE_V1_VERSION);
    qw_data_control_manager_v1::create(*m_server->handle());
    qw_ext_data_control_manager_v1::create(*m_server->handle(), EXT_DATA_CONTROL_MANAGER_V1_VERSION);
    qw_alpha_modifier_v1::create(*m_server->handle());
    auto *foreignRegistry = qw_xdg_foreign_registry::create(*m_server->handle());
    qw_xdg_foreign_v2::create(*m_server->handle(), *foreignRegistry);

    m_idleNotifier = qw_idle_notifier_v1::create(*m_server->handle());

    m_idleInhibitManager = qw_idle_inhibit_manager_v1::create(*m_server->handle());
    connect(m_idleInhibitManager, &qw_idle_inhibit_manager_v1::notify_new_inhibitor, this, &Helper::onNewIdleInhibitor);

    m_activationManagerV1 = m_server->attach<ActivationManagerInterfaceV1>(
        [this](WSurface *surface, WSeat *seat) -> bool {
            // Determine whether the surface can transfer activation for the same seat
            // that produced the serial in set_serial.
            if (!seat || !seat->isValid()) {
                return false;
            }

            auto wrapper = m_rootSurfaceContainer->getSurface(surface);
            if (!wrapper) {
                return false;
            }

            if (seat->keyboardFocusSurface() == surface) {
                return true;
            }

            if (seat->pointerFocusSurface() == surface) {
                return true;
            }

            if (wrapper->isActivated() && getLastInteractingSeat(wrapper) == seat) {
                return true;
            }

            return false;
        });
    connect(m_activationManagerV1,
            &ActivationManagerInterfaceV1::activateRequested,
            this,
            [this](ActivationManagerInterfaceV1::TokenDisposition disposition, WSurface *wsurface, WSeat *seat) {
                auto wrapper = m_rootSurfaceContainer->getSurface(wsurface);
                if (!wrapper) {
                    qCWarning(lcTlCore) << "Activation request for unknown surface!";
                    return;
                }
                // Don't use hasActiveCapability() here — it also checks UnMinimized,
                // but minimized windows should be allowed to activate (which unminimizes them).
                if (!wsurface->mapped()) {
                    qCWarning(lcTlCore) << "Activation request for unmapped surface!";
                    return;
                }
                if (!wrapper->hasInitializeContainer()) {
                    qCWarning(lcTlCore) << "Activation request for surface without initialized container:"
                                       << "container =" << wrapper->container();
                    return;
                }
                switch (disposition) {
                case ActivationManagerInterfaceV1::TokenDisposition::Active:
                    forceActivateSurface(wrapper, Qt::OtherFocusReason, seat);
                    break;
                case ActivationManagerInterfaceV1::TokenDisposition::Attention:
                    wrapper->setAttention(true);
                    break;
                case ActivationManagerInterfaceV1::TokenDisposition::Invalid:
                    // Use a relaxed policy: fallback to attention if the token is invalid
                    wrapper->setAttention(true);
                    break;
                }
            });

    m_screensaverInterfaceV1 = m_server->attach<ScreensaverInterfaceV1>();

    m_outputPowerManager = qw_output_power_manager_v1::create(*m_server->handle());

    connect(m_outputPowerManager, &qw_output_power_manager_v1::notify_set_mode, this, &Helper::onSetOutputPowerMode);
#ifdef EXT_SESSION_LOCK_V1
    m_sessionLockManager = m_server->attach<WSessionLockManager>();
    if (!m_lockScreen) {
        setLockScreenImpl(nullptr);
    }
    connect(m_sessionLockManager,
            &WSessionLockManager::lockCreated,
            this,
            &Helper::onExtSessionLock);
#endif

    m_wallpaperNotifierInterfaceV1 = m_server->attach<TreelandWallpaperNotifierInterfaceV1>();
    if (isDDMDisplay()) {
        m_wallpaperNotifierInterfaceV1->setFilter([this](WClient *client) { return m_sessionManager->isDDEUserClient(client); });
    }
    connect(m_wallpaperNotifierInterfaceV1,
            &TreelandWallpaperNotifierInterfaceV1::bound,
            m_wallpaperManager,
            &WallpaperManager::onWallpaperNotifierBound);

    m_wallpaperManagerInterfaceV1 = m_server->attach<TreelandWallpaperManagerInterfaceV1>();
    connect(m_wallpaperManagerInterfaceV1,
            &TreelandWallpaperManagerInterfaceV1::wallpaperCreated,
            m_wallpaperManager,
            &WallpaperManager::onWallpaperAdded);

    m_shortcutManager = m_server->attach<ShortcutManagerV2>();
    connect(m_treeland,
            &Treeland::Treeland::SessionChanged,
            m_shortcutManager,
            &ShortcutManagerV2::onSessionChanged);
    auto shortcutControl = m_shortcutManager->controller();
    auto *shortcutRunner = new ShortcutRunner(shortcutControl);
    connect(shortcutControl,
            &ShortcutController::actionTriggered,
            shortcutRunner,
            &ShortcutRunner::onActionTrigger);
    connect(shortcutControl,
            &ShortcutController::actionProgress,
            shortcutRunner,
            &ShortcutRunner::onActionProgress);
    connect(shortcutControl,
            &ShortcutController::actionFinished,
            shortcutRunner,
            &ShortcutRunner::onActionFinish);

    m_inputManagerInterfaceV1 = m_server->attach<TreelandInputManagerInterfaceV1>();
    connect(m_inputManagerInterfaceV1,
            &TreelandInputManagerInterfaceV1::mouseSettingsCreated,
            m_inputManager,
            &InputManager::onMouseSettingsCreated);
    connect(m_inputManagerInterfaceV1,
            &TreelandInputManagerInterfaceV1::touchpadSettingsCreated,
            m_inputManager,
            &InputManager::onTouchpadSettingsCreated);
    connect(m_inputManagerInterfaceV1,
            &TreelandInputManagerInterfaceV1::keyboardSettingsCreated,
            m_inputManager,
            &InputManager::onKeyboardSettingsCreated);

    m_keyboardStateNotifyManagerInterfaceV1 = m_server->attach<TreelandKeyboardStateNotifyManagerInterfaceV1>();

#if TREELANDCONFIG_DCONFIG_FILE_VERSION_MINOR > 0
    if (m_globalConfig->isInitializeSucceeded()) {
#else
    if (m_globalConfig->isInitializeSucceed()) {
#endif
    } else {
    }

    m_backend->handle()->start();

    // If the stored primary output is no longer present, select a random one as primary
    const QString primaryOutputId = m_globalConfig->primaryOutputId();
    if (!currentPrimaryMatchesId(m_rootSurfaceContainer, primaryOutputId)) {
        const auto &outputs = m_rootSurfaceContainer->outputs();
        if (!outputs.isEmpty()) {
            int randomIndex = QRandomGenerator::global()->bounded(outputs.size());
            m_rootSurfaceContainer->setPrimaryOutput(outputs.at(randomIndex));
        }
    }
}

SeatsManager *Helper::seatManager() const
{
    return m_seatManager;
}

WSeat *Helper::getSeatForEvent(QInputEvent *event) const
{
    return m_seatManager->getSeatForEvent(event);
}

void Helper::activateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason, WSeat *seat)
{
    if (wrapper && wrapper->type() == SurfaceWrapper::Type::XWayland) {
        qCDebug(lcTlXwayland) << "[XWL_ACTIVATE] XWayland activation requested:"
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "reason=" << reason
                               << "seat=" << seat
                               << "has_active_capability=" << wrapper->hasActiveCapability()
                               << "has_focus_capability=" << wrapper->hasFocusCapability();
    }

    if (wrapper && wrapper->isIMCandidatePanel())
        return;

    if (wrapper && wrapper->isXWaylandPopupLikeTransient() && reason == Qt::MouseFocusReason) {
        auto *parentWrapper = xwaylandPopupTransientParentWrapper(wrapper, seat);
        qCDebug(lcTlXwayland) << "[XWL_POPUP_MOUSE_ACTIVATE_SKIP] Keep XWayland popup mouse activation pointer-only:"
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "parentWrapper=" << parentWrapper
                               << "seat=" << seat
                               << "reason=" << reason
                               << "geometry=" << wrapper->geometry();
        return;
    }

    // Plain activation: if the deepest modal is minimized, refuse to activate the parent
    // entirely. The user must explicitly unminimize the modal first (e.g., click it).
    SurfaceWrapper *originalWrapper = wrapper;
    if (wrapper) {
        if (SurfaceWrapper *modal = wrapper->findModal()) {
            if (modal != wrapper) {
                if (modal->workspaceId() != wrapper->workspaceId()
                    && wrapper->workspaceId() != -1) {
                    workspace()->moveSurfaceTo(modal, wrapper->workspaceId());
                }

                if (modal->isMinimized()) {
                    qCCritical(lcTlShell) << "Refusing to activate parent with minimized modal"
                                          << "parent =" << wrapper << "modal =" << modal;
                    return;
                }

                originalWrapper->stackToLast();
                wrapper = modal;
            }
        }
    }

    if (m_blockActivateSurface && wrapper && wrapper->type() != SurfaceWrapper::Type::LockScreen) {
        if (wrapper->hasActiveCapability()) {
            workspace()->pushActivedSurface(wrapper);
        }
        return;
    }

    if (!wrapper || wrapper->hasActiveCapability()) {
        setActivatedSurface(wrapper, seat);
    } else {
        qCCritical(lcTlShell)
            << "Trying to activate a surface which doesn't have ActiveCapability!";
    }

    if (!wrapper || wrapper->hasFocusCapability()) {
        requestKeyboardFocus(wrapper, reason, seat);
    }
}

SurfaceWrapper *Helper::activeXWaylandPopupPointerOwner(WSeat *seat) const
{
    if (!seat)
        return nullptr;

    const auto it = m_xwaylandPopupPointerOwners.constFind(seat);
    if (it == m_xwaylandPopupPointerOwners.constEnd())
        return nullptr;

    auto *wrapper = qobject_cast<SurfaceWrapper *>(it->wrapper.data());
    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient() || !wrapper->surface()
        || !wrapper->surface()->mapped()) {
        return nullptr;
    }

    return wrapper;
}

SurfaceWrapper *Helper::findXWaylandPopupPointerTarget(WSeat *seat,
                                                       SurfaceWrapper *eventTarget,
                                                       QInputEvent *event,
                                                       QPointF *targetLocalPos) const
{
    if (targetLocalPos)
        *targetLocalPos = QPointF();

    if (!seat || !event || !isXWaylandPopupPointerEvent(event) || !event->isSinglePointEvent())
        return nullptr;

    const QPointF scenePos = pointerEventScenePosition(event);
    const auto x11GlobalFallbackReasonFor = [&](SurfaceWrapper *candidate) {
        if (!candidate)
            return X11GlobalRequestFallbackReason::None;

        auto *candidateXWaylandSurface = xwaylandSurfaceForWrapper(candidate);
        if (!candidateXWaylandSurface)
            return X11GlobalRequestFallbackReason::None;

        if (!eventTarget || eventTarget->type() != SurfaceWrapper::Type::XWayland) {
            if (candidate == activeXWaylandPopupPointerOwner(seat))
                return X11GlobalRequestFallbackReason::ActivePointerOwner;
            return X11GlobalRequestFallbackReason::None;
        }

        auto *eventTargetXWaylandSurface = xwaylandSurfaceForWrapper(eventTarget);
        if (!eventTargetXWaylandSurface)
            return X11GlobalRequestFallbackReason::None;

        if (candidate == eventTarget)
            return X11GlobalRequestFallbackReason::EventTarget;

        if (candidate->parentSurface() == eventTarget)
            return X11GlobalRequestFallbackReason::ParentWrapper;

        if (xwaylandPopupTransientParentWrapper(candidate, seat) == eventTarget)
            return X11GlobalRequestFallbackReason::ResolvedParentWrapper;

        auto *parentXWaylandSurface = candidateXWaylandSurface->parentXWaylandSurface();
        for (int depth = 0; parentXWaylandSurface && depth < 32;
             ++depth, parentXWaylandSurface = parentXWaylandSurface->parentXWaylandSurface()) {
            if (parentXWaylandSurface == eventTargetXWaylandSurface
                || xwaylandWindowId(parentXWaylandSurface)
                    == xwaylandWindowId(eventTargetXWaylandSurface)) {
                return X11GlobalRequestFallbackReason::X11ParentChain;
            }
        }

        return X11GlobalRequestFallbackReason::None;
    };
    const auto logHitTest = [&](const char *stage,
                                SurfaceWrapper *candidate,
                                const QPointF &localPos,
                                bool contains,
                                bool usedX11RequestPosition,
                                bool usedX11GlobalRequestPosition,
                                bool usedX11EffectiveRequestGeometry,
                                X11GlobalRequestFallbackReason x11GlobalFallbackReason,
                                bool usedGlobalItemPosition) {
        auto *xwaylandSurface = xwaylandSurfaceForWrapper(candidate);
        auto *parentXWaylandSurface =
            xwaylandSurface ? xwaylandSurface->parentXWaylandSurface() : nullptr;
        auto *surfaceItem = candidate ? candidate->surfaceItem() : nullptr;
        auto *eventItem = surfaceItem ? surfaceItem->eventItem() : nullptr;
        const QPointF itemLocalPos = eventItem ? eventItem->mapFromScene(scenePos) : QPointF();
        const bool itemContains = eventItem ? eventItem->contains(itemLocalPos) : false;
        const bool inputRegionContains = candidate && candidate->surface()
            ? candidate->surface()->inputRegionContains(itemLocalPos)
            : false;
        QPointF x11LocalPos;
        QPointF x11GlobalLocalPos;
        bool x11Contains = false;
        bool x11GlobalContains = false;
        bool x11UsedEffectiveRequestGeometry = false;
        QRectF x11RequestGeometry;
        X11RequestGeometryInfo x11RequestInfo;
        const bool hasX11RequestPosition =
            xwaylandRequestPointerLocalPosition(candidate,
                                                seat,
                                                event,
                                                &x11LocalPos,
                                                &x11Contains,
                                                &x11RequestGeometry,
                                                &x11GlobalLocalPos,
                                                &x11GlobalContains,
                                                &x11UsedEffectiveRequestGeometry,
                                                &x11RequestInfo);
        const qreal surfaceSizeRatio = xwaylandSurfaceSizeRatio(candidate);
        const auto x11RequestFlags = xwaylandSurface ? xwaylandSurface->requestConfigureFlags()
                                                     : WXWaylandSurface::ConfigureFlags();
        const QRectF x11SurfaceGeometry =
            xwaylandSurface ? QRectF(xwaylandSurface->geometry()) : QRectF();
        const QPointF scaledScenePos(scenePos.x() * surfaceSizeRatio,
                                     scenePos.y() * surfaceSizeRatio);
        const QRectF x11EffectiveRequestGeometry =
            x11RequestInfo.valid ? x11RequestInfo.effectiveGeometry : QRectF();
        const QPointF x11EffectiveLocalPos = x11RequestInfo.valid
            ? scaledScenePos - x11EffectiveRequestGeometry.topLeft()
            : QPointF();
        const QPointF x11EffectiveGlobalLocalPos = x11RequestInfo.valid
            ? pointerCursorGlobalPosition(seat, event) - x11EffectiveRequestGeometry.topLeft()
            : QPointF();
        const QRectF x11EffectiveLocalRect(QPointF(), x11EffectiveRequestGeometry.size());
        const bool x11EffectiveContains =
            x11RequestInfo.valid && x11EffectiveLocalRect.contains(x11EffectiveLocalPos);
        const bool x11EffectiveGlobalContains =
            x11RequestInfo.valid && x11EffectiveLocalRect.contains(x11EffectiveGlobalLocalPos);
        const auto globalFallback =
            x11GlobalRequestFallbackDecision(x11GlobalFallbackReason,
                                             itemContains,
                                             inputRegionContains,
                                             hasX11RequestPosition,
                                             x11RequestGeometry,
                                             x11LocalPos,
                                             x11Contains,
                                             x11GlobalLocalPos,
                                             x11GlobalContains);
        const auto globalHit = xwaylandPopupGlobalHitTest(candidate, seat, event);
        const char *hitSource = "none";
        if (contains) {
            if (usedGlobalItemPosition) {
                hitSource = globalHit.inputRegionContains && !globalHit.itemContains
                    ? "global-input-region"
                    : "global-item";
            } else if (usedX11GlobalRequestPosition) {
                hitSource = usedX11EffectiveRequestGeometry
                    ? "x11-request-parent-chain-global"
                    : "x11-request-global";
            } else if (usedX11RequestPosition) {
                hitSource = globalFallback.sceneEdgeAllowed
                    ? "x11-request-scene-edge"
                    : (usedX11EffectiveRequestGeometry ? "x11-request-parent-chain"
                                                       : "x11-request-scene");
            } else if (inputRegionContains && !itemContains) {
                hitSource = "input-region";
            } else {
                hitSource = "item";
            }
        }
        qCDebug(lcTlXwayland) << "[XWL_POPUP_HIT_TEST] XWayland popup pointer hit test:"
                               << "stage=" << stage
                               << "event_type=" << event->type()
                               << "window_id=" << xwaylandWindowId(candidate)
                               << "parent_window_id=" << xwaylandWindowId(parentXWaylandSurface)
                               << "candidate=" << candidate
                               << "eventTarget=" << eventTarget
                               << "event_target_window_id=" << xwaylandWindowId(eventTarget)
                               << "event_target_type="
                               << (eventTarget ? static_cast<int>(eventTarget->type()) : -1)
                               << "event_target_shell_surface="
                               << (eventTarget ? eventTarget->shellSurface() : nullptr)
                               << "event_target_popup_like="
                               << (eventTarget ? eventTarget->isXWaylandPopupLikeTransient() : false)
                               << "seat=" << seat
                               << "scene_pos=" << scenePos
                               << "global_pos=" << pointerEventGlobalPosition(event)
                               << "local_pos=" << localPos
                               << "contains=" << contains
                               << "hit_source=" << hitSource
                               << "item_local_pos=" << itemLocalPos
                               << "item_contains=" << itemContains
                               << "input_region_contains=" << inputRegionContains
                               << "used_global_item_position=" << usedGlobalItemPosition
                               << "cursor_pos=" << globalHit.cursorPos
                               << "global_item_local_pos=" << globalHit.localPos
                               << "global_item_contains=" << globalHit.itemContains
                               << "global_input_region_contains="
                               << globalHit.inputRegionContains
                               << "global_item_geometry=" << globalHit.itemGlobalGeometry
                               << "used_x11_request_position=" << usedX11RequestPosition
                               << "used_x11_global_request_position="
                               << usedX11GlobalRequestPosition
                               << "used_x11_effective_request_geometry="
                               << usedX11EffectiveRequestGeometry
                               << "x11_effective_request_geometry="
                               << x11EffectiveRequestGeometry
                               << "x11_effective_scene_local_pos="
                               << x11EffectiveLocalPos
                               << "x11_effective_contains="
                               << x11EffectiveContains
                               << "x11_effective_global_local_pos="
                               << x11EffectiveGlobalLocalPos
                               << "x11_effective_global_contains="
                               << x11EffectiveGlobalContains
                               << "x11_unwrapped_parent_offset="
                               << x11RequestInfo.unwrappedParentOffset
                               << "x11_unwrapped_parent_offset_used="
                               << x11RequestInfo.usedUnwrappedParentOffset
                               << "x11_direct_parent_window_id="
                               << x11RequestInfo.directParentWindowId
                               << "x11_direct_parent_geometry="
                               << x11RequestInfo.directParentGeometry
                               << "x11_effective_parent_window_id="
                               << x11RequestInfo.effectiveParentWindowId
                               << "x11_effective_parent_geometry="
                               << x11RequestInfo.effectiveParentGeometry
                               << "x11_unwrapped_parent_depth="
                               << x11RequestInfo.unwrappedParentDepth
                               << "x11_parent_chain_truncated="
                               << x11RequestInfo.parentChainTruncated
                               << "allow_x11_global_request_position="
                               << globalFallback.allowed
                               << "x11_global_request_fallback_reason="
                               << x11GlobalRequestFallbackReasonName(x11GlobalFallbackReason)
                               << "x11_global_request_reject_reason="
                               << globalFallback.rejectReason
                               << "x11_scene_request_miss_distance="
                               << globalFallback.sceneMissDistance
                               << "x11_scene_edge_hit_threshold="
                               << globalFallback.sceneEdgeHitThreshold
                               << "x11_scene_edge_hit_allowed="
                               << globalFallback.sceneEdgeAllowed
                               << "x11_global_request_miss_threshold="
                               << globalFallback.missThreshold
                               << "x11_global_request_hit_ignored="
                               << (x11GlobalFallbackReason != X11GlobalRequestFallbackReason::None
                                   && x11GlobalContains && !x11Contains && !itemContains
                                   && !inputRegionContains && !usedX11GlobalRequestPosition
                                   && !globalFallback.sceneEdgeAllowed)
                               << "has_x11_request_position=" << hasX11RequestPosition
                               << "x11_request_flags=" << static_cast<int>(x11RequestFlags)
                               << "x11_request_geometry=" << x11RequestGeometry
                               << "x11_raw_request_geometry=" << x11RequestInfo.rawGeometry
                               << "x11_surface_geometry=" << x11SurfaceGeometry
                               << "x11_scene_local_pos=" << x11LocalPos
                               << "x11_contains=" << x11Contains
                               << "x11_global_local_pos=" << x11GlobalLocalPos
                               << "x11_global_contains=" << x11GlobalContains
                               << "surface_size_ratio="
                               << surfaceSizeRatio
                               << "input_model="
                               << xwaylandInputModelName(xwaylandSurface
                                                             ? xwaylandSurface->inputModel()
                                                             : WXWaylandSurface::InputModelNone)
                               << "wrapper_geometry="
                               << (candidate ? candidate->geometry() : QRectF());
    };

    SurfaceWrapper *owner = activeXWaylandPopupPointerOwner(seat);
    bool ownerContains = false;
    QPointF ownerLocalPos;
    bool ownerUsedX11RequestPosition = false;
    bool ownerUsedX11GlobalRequestPosition = false;
    bool ownerUsedX11EffectiveRequestGeometry = false;
    bool ownerUsedGlobalItemPosition = false;
    bool ownerHasPressedSequence = false;
    bool ownerPointerHasGrab = false;
    bool ownerPointerFocusContinuationAllowed = false;
    const char *ownerPointerFocusContinuationReason = "no-owner";
    auto logOwnerPointerFocusContinuation = [&](const char *action, const char *reason) {
        auto *ownerEventItem = owner && owner->surfaceItem()
            ? owner->surfaceItem()->eventItem()
            : nullptr;
        const bool ownerItemContains =
            ownerEventItem ? ownerEventItem->contains(ownerLocalPos) : false;
        const bool ownerInputRegionContains = owner && owner->surface()
            ? owner->surface()->inputRegionContains(ownerLocalPos)
            : false;
        const auto ownerIt = m_xwaylandPopupPointerOwners.constFind(seat);
        auto *storedParentWrapper =
            ownerIt != m_xwaylandPopupPointerOwners.constEnd()
            ? qobject_cast<SurfaceWrapper *>(ownerIt->parentWrapper.data())
            : nullptr;

        qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_OWNER_CONTINUE] XWayland popup pointer owner continuation:"
                               << "action=" << action
                               << "reason=" << reason
                               << "event_type=" << event->type()
                               << "window_id=" << xwaylandWindowId(owner)
                               << "owner=" << owner
                               << "owner_surface=" << (owner ? owner->surface() : nullptr)
                               << "pointer_focus_surface="
                               << (seat ? seat->pointerFocusSurface() : nullptr)
                               << "eventTarget=" << eventTarget
                               << "event_target_window_id=" << xwaylandWindowId(eventTarget)
                               << "event_target_type="
                               << (eventTarget ? static_cast<int>(eventTarget->type()) : -1)
                               << "event_target_shell_surface="
                               << (eventTarget ? eventTarget->shellSurface() : nullptr)
                               << "stored_parentWrapper=" << storedParentWrapper
                               << "resolved_parentWrapper="
                               << xwaylandPopupTransientParentWrapper(owner, seat)
                               << "seat=" << seat
                               << "local_pos=" << ownerLocalPos
                               << "item_contains=" << ownerItemContains
                               << "input_region_contains=" << ownerInputRegionContains
                               << "hit_contains=" << ownerContains
                               << "has_pressed_sequence=" << ownerHasPressedSequence
                               << "pointer_has_grab=" << ownerPointerHasGrab
                               << "used_x11_request_position="
                               << ownerUsedX11RequestPosition
                               << "used_global_item_position="
                               << ownerUsedGlobalItemPosition
                               << "used_x11_global_request_position="
                               << ownerUsedX11GlobalRequestPosition
                               << "used_x11_effective_request_geometry="
                               << ownerUsedX11EffectiveRequestGeometry
                               << "scene_pos=" << scenePos
                               << "global_pos=" << pointerEventGlobalPosition(event)
                               << "owner_geometry="
                               << (owner ? owner->geometry() : QRectF());
    };
    if (owner) {
        const auto ownerIt = m_xwaylandPopupPointerOwners.constFind(seat);
        ownerHasPressedSequence =
            ownerIt != m_xwaylandPopupPointerOwners.constEnd() && ownerIt->pressedButtons > 0;
        ownerPointerHasGrab = seat->pointerHasGrab();
        const X11GlobalRequestFallbackReason ownerGlobalFallbackReason =
            x11GlobalFallbackReasonFor(owner);
        popupPointerLocalPosition(owner,
                                  seat,
                                  event,
                                  &ownerLocalPos,
                                  &ownerContains,
                                  &ownerUsedX11RequestPosition,
                                  ownerGlobalFallbackReason,
                                  &ownerUsedX11GlobalRequestPosition,
                                  &ownerUsedX11EffectiveRequestGeometry);
        if (!ownerContains) {
            const auto globalHit = xwaylandPopupGlobalHitTest(owner, seat, event);
            if (globalHit.contains) {
                ownerLocalPos = globalHit.localPos;
                ownerContains = true;
                ownerUsedGlobalItemPosition = true;
            }
        }
        logHitTest("active-owner",
                   owner,
                   ownerLocalPos,
                   ownerContains,
                   ownerUsedX11RequestPosition,
                   ownerUsedX11GlobalRequestPosition,
                   ownerUsedX11EffectiveRequestGeometry,
                   ownerGlobalFallbackReason,
                   ownerUsedGlobalItemPosition);
        if (ownerHasPressedSequence) {
            if (targetLocalPos)
                *targetLocalPos = ownerLocalPos;
            return owner;
        }

        if (!owner->surfaceItem() || !owner->surfaceItem()->eventItem()) {
            ownerPointerFocusContinuationReason = "missing-event-item";
        } else if (!owner->surface()) {
            ownerPointerFocusContinuationReason = "missing-surface";
        } else if (seat->pointerFocusSurface() != owner->surface()) {
            ownerPointerFocusContinuationReason = "pointer-focus-not-owner";
        } else if (eventTarget && eventTarget->type() == SurfaceWrapper::Type::XWayland) {
            ownerPointerFocusContinuationReason = "xwayland-event-target";
        } else if (workspace() && workspace()->current()
                   && !owner->showOnWorkspace(workspace()->current()->id())) {
            ownerPointerFocusContinuationReason = "not-current-workspace";
        } else if (!ownerContains && !ownerPointerHasGrab) {
            ownerPointerFocusContinuationReason = "outside-without-pointer-grab";
        } else {
            ownerPointerFocusContinuationAllowed = true;
            ownerPointerFocusContinuationReason =
                ownerPointerHasGrab ? "pointer-grab-owner" : "pointer-focus-owner";
        }
    }

    if (!m_rootSurfaceContainer) {
        if (ownerContains && targetLocalPos)
            *targetLocalPos = ownerLocalPos;
        if (ownerContains)
            return owner;
        return nullptr;
    }

    const auto surfaces = m_rootSurfaceContainer->surfaces();
    for (auto it = surfaces.crbegin(); it != surfaces.crend(); ++it) {
        auto *candidate = *it;
        if (!candidate || candidate == eventTarget || !candidate->isXWaylandPopupLikeTransient()
            || !candidate->surface() || !candidate->surface()->mapped()
            || !candidate->showOnWorkspace(workspace()->current()->id())) {
            continue;
        }

        QPointF localPos;
        bool contains = false;
        bool usedX11RequestPosition = false;
        bool usedX11GlobalRequestPosition = false;
        bool usedX11EffectiveRequestGeometry = false;
        const X11GlobalRequestFallbackReason x11GlobalFallbackReason =
            x11GlobalFallbackReasonFor(candidate);
        if (!popupPointerLocalPosition(candidate,
                                       seat,
                                       event,
                                       &localPos,
                                       &contains,
                                       &usedX11RequestPosition,
                                       x11GlobalFallbackReason,
                                       &usedX11GlobalRequestPosition,
                                       &usedX11EffectiveRequestGeometry)) {
            logHitTest("candidate-map-failed",
                       candidate,
                       localPos,
                       contains,
                       false,
                       false,
                       false,
                       x11GlobalFallbackReason,
                       false);
            continue;
        }
        bool usedGlobalItemPosition = false;
        if (!contains) {
            const auto globalHit = xwaylandPopupGlobalHitTest(candidate, seat, event);
            if (globalHit.contains) {
                localPos = globalHit.localPos;
                contains = true;
                usedGlobalItemPosition = true;
            }
        }

        logHitTest("candidate",
                   candidate,
                   localPos,
                   contains,
                   usedX11RequestPosition,
                   usedX11GlobalRequestPosition,
                   usedX11EffectiveRequestGeometry,
                   x11GlobalFallbackReason,
                   usedGlobalItemPosition);
        if (!contains)
            continue;

        if (targetLocalPos)
            *targetLocalPos = localPos;
        return candidate;
    }

    if (ownerContains) {
        if (targetLocalPos)
            *targetLocalPos = ownerLocalPos;
        return owner;
    }

    if (ownerPointerFocusContinuationAllowed) {
        logOwnerPointerFocusContinuation("continue", ownerPointerFocusContinuationReason);
        if (targetLocalPos)
            *targetLocalPos = ownerLocalPos;
        return owner;
    }

    if (owner)
        logOwnerPointerFocusContinuation("skip", ownerPointerFocusContinuationReason);

    logHitTest("miss",
               nullptr,
               QPointF(),
               false,
               false,
               false,
               false,
               X11GlobalRequestFallbackReason::None,
               false);
    return nullptr;
}

SurfaceWrapper *Helper::activeXWaylandPointerButtonSequenceWrapper(WSeat *seat) const
{
    return xwaylandPointerButtonSequenceWrapper(seat, true);
}

SurfaceWrapper *Helper::xwaylandPointerButtonSequenceWrapper(WSeat *seat, bool requireMapped) const
{
    if (!seat)
        return nullptr;

    const auto it = m_xwaylandPointerButtonSequences.constFind(seat);
    if (it == m_xwaylandPointerButtonSequences.constEnd() || it->pressedButtons <= 0)
        return nullptr;

    auto *wrapper = qobject_cast<SurfaceWrapper *>(it->wrapper.data());
    if (!wrapper || !wrapper->surface())
        return nullptr;

    if (requireMapped && !wrapper->surface()->mapped())
        return nullptr;

    return wrapper;
}

bool Helper::xwaylandPointerButtonSequenceBelongsTo(WSeat *seat, SurfaceWrapper *wrapper) const
{
    return wrapper && activeXWaylandPointerButtonSequenceWrapper(seat) == wrapper;
}

bool Helper::xwaylandPointerButtonSequenceNeedsRecovery(WSeat *seat) const
{
    if (!seat)
        return false;

    const auto it = m_xwaylandPointerButtonSequences.constFind(seat);
    if (it == m_xwaylandPointerButtonSequences.constEnd() || it->pressedButtons <= 0)
        return false;

    auto *sequenceWrapper = qobject_cast<SurfaceWrapper *>(it->wrapper.data());
    const bool sequenceMapped =
        sequenceWrapper && sequenceWrapper->surface() && sequenceWrapper->surface()->mapped();
    return it->pendingUnmapRelease || !sequenceWrapper || (it->popupLike && !sequenceMapped);
}

bool Helper::xwaylandPointerButtonSequenceBlocksPopup(SurfaceWrapper *wrapper, WSeat *seat) const
{
    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient())
        return false;

    auto *sequenceWrapper = activeXWaylandPointerButtonSequenceWrapper(seat);
    return sequenceWrapper && sequenceWrapper != wrapper;
}

bool Helper::xwaylandPointerButtonSequenceAllowsRelatedPopupMove(SurfaceWrapper *wrapper,
                                                                 WSeat *seat,
                                                                 QInputEvent *event) const
{
    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient() || !seat || !event)
        return false;
    if (!wrapper->surface() || !wrapper->surface()->mapped() || !event->isSinglePointEvent())
        return false;

    if (!isXWaylandPopupPointerEnterOrMove(event)
        || xwaylandPopupPointerEventNeedsButtonSequence(event)) {
        return false;
    }

    auto *sequenceWrapper = activeXWaylandPointerButtonSequenceWrapper(seat);
    if (!sequenceWrapper || sequenceWrapper == wrapper
        || !sequenceWrapper->isXWaylandPopupLikeTransient()) {
        return false;
    }

    const auto sequenceIt = m_xwaylandPointerButtonSequences.constFind(seat);
    auto *storedSequenceParent =
        sequenceIt != m_xwaylandPointerButtonSequences.constEnd()
        ? qobject_cast<SurfaceWrapper *>(sequenceIt->parentWrapper.data())
        : nullptr;
    auto *wrapperParent = xwaylandPopupTransientParentWrapper(wrapper, seat);
    auto *sequenceParent = xwaylandPopupTransientParentWrapper(sequenceWrapper, seat);
    if (wrapperParent && sequenceParent && wrapperParent == sequenceParent)
        return true;
    if (wrapperParent && storedSequenceParent && wrapperParent == storedSequenceParent)
        return true;
    if ((wrapperParent && wrapperParent == sequenceWrapper)
        || (sequenceParent && sequenceParent == wrapper)
        || (storedSequenceParent && storedSequenceParent == wrapper)) {
        return true;
    }

    auto *xwaylandSurface = xwaylandSurfaceForWrapper(wrapper);
    auto *sequenceXWaylandSurface = xwaylandSurfaceForWrapper(sequenceWrapper);
    if (!xwaylandSurface || !sequenceXWaylandSurface)
        return false;

    const auto sameXWaylandSurface = [](WXWaylandSurface *lhs, WXWaylandSurface *rhs) {
        if (!lhs || !rhs)
            return false;
        if (lhs == rhs)
            return true;

        const uint32_t lhsWindowId = xwaylandWindowId(lhs);
        return lhsWindowId != 0 && lhsWindowId == xwaylandWindowId(rhs);
    };
    const auto hasXWaylandAncestor = [&](WXWaylandSurface *surface,
                                         WXWaylandSurface *ancestor) {
        if (!surface || !ancestor)
            return false;

        auto *parent = surface->parentXWaylandSurface();
        for (int depth = 0; parent && depth < 32;
             ++depth, parent = parent->parentXWaylandSurface()) {
            if (sameXWaylandSurface(parent, ancestor))
                return true;
            if (parent == surface || parent->isInvalidated())
                return false;
        }

        return false;
    };

    if (hasXWaylandAncestor(xwaylandSurface, sequenceXWaylandSurface)
        || hasXWaylandAncestor(sequenceXWaylandSurface, xwaylandSurface)) {
        return true;
    }

    return sameXWaylandSurface(xwaylandSurface->parentXWaylandSurface(),
                               sequenceXWaylandSurface->parentXWaylandSurface());
}

SurfaceWrapper *Helper::xwaylandPopupTransientParentWrapper(SurfaceWrapper *wrapper, WSeat *seat) const
{
    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient())
        return nullptr;

    if (auto *parentWrapper = wrapper->parentSurface())
        return parentWrapper;

    SurfaceWrapper *activeSurface = nullptr;
    if (m_rootSurfaceContainer) {
        if (auto *seatContainer = m_rootSurfaceContainer->getSeatContainerOrDefault(seat))
            activeSurface = seatContainer->activatedSurface();
    }
    if (!activeSurface)
        activeSurface = activatedSurface();

    if (activeSurface && activeSurface != wrapper && activeSurface->type() == SurfaceWrapper::Type::XWayland)
        return activeSurface;

    return nullptr;
}

bool Helper::xwaylandPopupUsesDeferredPointerFocus(SurfaceWrapper *wrapper) const
{
    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient())
        return false;

    if (xwaylandPopupPointerGrabOverridesParentFocus(wrapper))
        return false;

    auto *xwaylandSurface = xwaylandSurfaceForWrapper(wrapper);
    if (!xwaylandSurface)
        return false;

    const auto inputModel = xwaylandSurface->inputModel();
    return inputModel == WXWaylandSurface::InputModelLocal
        || inputModel == WXWaylandSurface::InputModelPassive;
}

bool Helper::xwaylandPopupHasPointerGrabFocus(SurfaceWrapper *wrapper) const
{
    auto *xwaylandSurface = xwaylandSurfaceForWrapper(wrapper);
    return xwaylandSurface && m_xwaylandPointerGrabFocusSurfaces.contains(xwaylandSurface);
}

bool Helper::xwaylandPopupPointerGrabOverridesParentFocus(SurfaceWrapper *wrapper) const
{
    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient()
        || !xwaylandPopupHasPointerGrabFocus(wrapper)) {
        return false;
    }

    auto *xwaylandSurface = xwaylandSurfaceForWrapper(wrapper);
    if (!xwaylandSurface)
        return false;

    const auto windowTypes = xwaylandSurface->windowTypes();
    const WXWaylandSurface::WindowTypes menuLikeTypes =
        WXWaylandSurface::NET_WM_WINDOW_TYPE_TOOLTIP
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_DND
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_DROPDOWN_MENU
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_POPUP_MENU
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_COMBO
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_MENU
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_NOTIFICATION
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_SPLASH;
    const bool hasXWaylandParent = xwaylandSurface->parentXWaylandSurface()
        && xwaylandSurface->parentXWaylandSurface() != xwaylandSurface;

    return xwaylandSurface->inputModel() == WXWaylandSurface::InputModelLocal
        && !xwaylandSurface->isBypassManager()
        && (windowTypes & WXWaylandSurface::NET_WM_WINDOW_TYPE_UTILITY)
        && !(windowTypes & menuLikeTypes)
        && hasXWaylandParent;
}

bool Helper::xwaylandPopupKeepsParentFocusOnPointer(SurfaceWrapper *wrapper, WSeat *) const
{
    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient())
        return false;

    auto *xwaylandSurface = xwaylandSurfaceForWrapper(wrapper);
    if (!xwaylandSurface)
        return false;

    if (xwaylandPopupPointerGrabOverridesParentFocus(wrapper))
        return false;

    const auto windowTypes = xwaylandSurface->windowTypes();
    const WXWaylandSurface::WindowTypes menuLikeTypes =
        WXWaylandSurface::NET_WM_WINDOW_TYPE_TOOLTIP
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_DND
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_DROPDOWN_MENU
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_POPUP_MENU
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_COMBO
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_MENU
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_NOTIFICATION
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_SPLASH;

    const bool hasXWaylandParent = xwaylandSurface->parentXWaylandSurface()
        && xwaylandSurface->parentXWaylandSurface() != xwaylandSurface;
    return xwaylandSurface->inputModel() == WXWaylandSurface::InputModelLocal
        && !xwaylandSurface->isBypassManager()
        && (windowTypes & WXWaylandSurface::NET_WM_WINDOW_TYPE_UTILITY)
        && !(windowTypes & menuLikeTypes)
        && hasXWaylandParent;
}

bool Helper::offerXWaylandPopupDeferredTakeFocus(WXWaylandSurface *surface,
                                                 SurfaceWrapper *wrapper,
                                                 Qt::FocusReason reason,
                                                 WSeat *seat,
                                                 const char *source)
{
    if (!surface || !wrapper || !wrapper->isXWaylandPopupLikeTransient() || !seat)
        return false;

    const auto inputModel = surface->inputModel();
    const auto windowTypes = surface->windowTypes();
    const WXWaylandSurface::WindowTypes menuLikeTypes =
        WXWaylandSurface::NET_WM_WINDOW_TYPE_TOOLTIP
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_DND
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_DROPDOWN_MENU
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_POPUP_MENU
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_COMBO
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_MENU
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_NOTIFICATION
        | WXWaylandSurface::NET_WM_WINDOW_TYPE_SPLASH;
    const bool mapped = wrapper->surface() && wrapper->surface()->mapped();
    const bool bypassManager = surface->isBypassManager();
    const bool utilityType = windowTypes & WXWaylandSurface::NET_WM_WINDOW_TYPE_UTILITY;
    const bool menuLikeType = windowTypes & menuLikeTypes;
    const bool supportsTakeFocus = surface->supportsWmTakeFocus();
    const bool alreadyFocused = wrapper->surface()
        && seat->keyboardFocusSurface() == wrapper->surface();
    const bool pendingOffer = m_pendingXWaylandFocusOffers.contains(surface);

    auto logDecision = [&](const char *action, const char *why) {
        qCDebug(lcTlXwayland) << "[XWL_POPUP_FOCUS_DECISION] XWayland popup focus decision:"
                               << "action=" << action
                               << "why=" << why
                               << "source=" << source
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "surface=" << surface
                               << "wrapper=" << wrapper
                               << "parentWrapper="
                               << xwaylandPopupTransientParentWrapper(wrapper, seat)
                               << "seat=" << seat
                               << "reason=" << reason
                               << "mapped=" << mapped
                               << "input_model=" << xwaylandInputModelName(inputModel)
                               << "window_types=" << windowTypes
                               << "bypass_manager=" << bypassManager
                               << "utility_type=" << utilityType
                               << "menu_like_type=" << menuLikeType
                               << "pointer_grab_focus="
                               << xwaylandPopupHasPointerGrabFocus(wrapper)
                               << "supports_wm_take_focus=" << supportsTakeFocus
                               << "pending_offer=" << pendingOffer
                               << "already_focused=" << alreadyFocused
                               << "geometry=" << wrapper->geometry();
    };

    if (reason == Qt::MouseFocusReason
        && commitXWaylandPointerGrabPopupFocus(wrapper,
                                               reason,
                                               seat,
                                               "x11-pointer-grab-before-pointer")) {
        logDecision("commit-pointer-grab-focus-before-deferred-pointer",
                    "x11-pointer-grab-focus");
        return true;
    }

    if (reason == Qt::MouseFocusReason && xwaylandPopupKeepsParentFocusOnPointer(wrapper, seat)) {
        if (pendingOffer)
            m_pendingXWaylandFocusOffers.remove(surface);
        logDecision("skip-take-focus-before-deferred-pointer", "keep-parent-focus");
        return false;
    }

    const char *skipReason = nullptr;
    if (!mapped)
        skipReason = "unmapped";
    else if (inputModel != WXWaylandSurface::InputModelLocal)
        skipReason = "not-local-input";
    else if (bypassManager)
        skipReason = "bypass-manager";
    else if (!utilityType || menuLikeType)
        skipReason = "not-managed-utility";
    else if (!supportsTakeFocus)
        skipReason = "missing-wm-take-focus";
    else if (alreadyFocused)
        skipReason = "already-focused";
    else if (pendingOffer)
        skipReason = "offer-already-pending";

    if (skipReason) {
        const bool worthLoggingSkip = inputModel == WXWaylandSurface::InputModelLocal
            && !bypassManager && utilityType && !menuLikeType
            && !alreadyFocused && !pendingOffer;
        if (worthLoggingSkip)
            logDecision("skip-take-focus-before-deferred-pointer", skipReason);
        return pendingOffer;
    }

    XWaylandFocusOffer offer;
    offer.wrapper = wrapper;
    offer.seat = seat;
    offer.reason = reason;
    m_pendingXWaylandFocusOffers.insert(surface, offer);

    if (!surface->offerFocus()) {
        m_pendingXWaylandFocusOffers.remove(surface);
        logDecision("skip-take-focus-before-deferred-pointer", "offer-not-sent");
        return false;
    }

    logDecision("offer-take-focus-before-deferred-pointer", "sent");
    return true;
}

void Helper::setXWaylandPopupPointerOwner(SurfaceWrapper *wrapper, WSeat *seat, const char *reason)
{
    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient() || !seat)
        return;

    auto *parentWrapper = xwaylandPopupTransientParentWrapper(wrapper, seat);
    auto &owner = m_xwaylandPopupPointerOwners[seat];
    const bool sameOwner = owner.wrapper == wrapper;
    owner.wrapper = wrapper;
    owner.parentWrapper = parentWrapper;
    owner.seat = seat;
    if (!sameOwner) {
        owner.pressedButtons = 0;
    }

    qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_OWNER] Set XWayland popup pointer owner:"
                           << "reason=" << reason
                           << "window_id=" << xwaylandWindowId(wrapper)
                           << "wrapper=" << wrapper
                           << "parentWrapper=" << parentWrapper
                           << "seat=" << seat
                           << "same_owner=" << sameOwner
                           << "pressed_buttons=" << owner.pressedButtons
                           << "geometry=" << wrapper->geometry();
}

void Helper::updateXWaylandPopupPointerOwnerForEvent(SurfaceWrapper *wrapper,
                                                     WSeat *seat,
                                                     QInputEvent *event,
                                                     const char *reason)
{
    if (!wrapper || !seat || !event || !isXWaylandPopupPointerEvent(event))
        return;

    setXWaylandPopupPointerOwner(wrapper, seat, reason);
    auto ownerIt = m_xwaylandPopupPointerOwners.find(seat);
    if (ownerIt == m_xwaylandPopupPointerOwners.end())
        return;

    const int oldPressedButtons = ownerIt->pressedButtons;
    if (event->type() == QEvent::MouseButtonPress) {
        ownerIt->pressedButtons += 1;
    } else if (event->type() == QEvent::MouseButtonRelease && ownerIt->pressedButtons > 0) {
        ownerIt->pressedButtons -= 1;
    }

    qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_OWNER] Update XWayland popup pointer owner event:"
                           << "reason=" << reason
                           << "event_type=" << event->type()
                           << "window_id=" << xwaylandWindowId(wrapper)
                           << "wrapper=" << wrapper
                           << "seat=" << seat
                           << "pressed_buttons_before=" << oldPressedButtons
                           << "pressed_buttons_after=" << ownerIt->pressedButtons;
}

void Helper::clearXWaylandPopupPointerOwner(WXWaylandSurface *surface, const char *reason)
{
    for (auto it = m_xwaylandPopupPointerOwners.begin();
         it != m_xwaylandPopupPointerOwners.end();) {
        auto *wrapper = qobject_cast<SurfaceWrapper *>(it->wrapper.data());
        auto *xwaylandSurface =
            wrapper ? qobject_cast<WXWaylandSurface *>(wrapper->shellSurface()) : nullptr;
        if (xwaylandSurface != surface) {
            ++it;
            continue;
        }

        qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_OWNER] Clear XWayland popup pointer owner:"
                               << "reason=" << reason
                               << "window_id=" << xwaylandWindowId(surface)
                               << "surface=" << surface
                               << "wrapper=" << wrapper
                               << "seat=" << it.key()
                               << "pressed_buttons=" << it->pressedButtons;
        it = m_xwaylandPopupPointerOwners.erase(it);
    }
}

void Helper::updateXWaylandPointerButtonSequence(SurfaceWrapper *wrapper,
                                                 WSeat *seat,
                                                 QInputEvent *event,
                                                 const char *reason)
{
    if (!seat || !event || !isXWaylandPointerButtonEvent(event))
        return;

    if (event->type() == QEvent::MouseButtonPress && !wrapper)
        return;

    if (event->type() == QEvent::MouseButtonPress) {
        clearXWaylandPendingPointerButtonSequenceForPress(seat,
                                                          wrapper,
                                                          event,
                                                          reason);
        auto &sequence = m_xwaylandPointerButtonSequences[seat];
        const int oldPressedButtons = sequence.pressedButtons;
        auto *sequenceWrapper = qobject_cast<SurfaceWrapper *>(sequence.wrapper.data());
        if (sequence.pressedButtons <= 0 || !sequence.wrapper) {
            auto *parentWrapper = wrapper->parentSurface();
            if (!parentWrapper)
                parentWrapper = xwaylandPopupTransientParentWrapper(wrapper, seat);
            sequence.wrapper = wrapper;
            sequence.parentWrapper = parentWrapper;
            sequence.seat = seat;
            sequence.client = wrapper->surface() ? wrapper->surface()->waylandClient() : nullptr;
            sequence.popupLike = wrapper->isXWaylandPopupLikeTransient();
            sequence.pressedButtons = 0;
            sequence.pendingUnmapRelease = false;
            sequence.windowId = xwaylandWindowId(wrapper);
        }
        sequence.pressedButtons += 1;

        sequenceWrapper = qobject_cast<SurfaceWrapper *>(sequence.wrapper.data());
        qCDebug(lcTlXwayland) << "[XWL_POINTER_SEQUENCE] Update XWayland pointer button sequence:"
                               << "reason=" << reason
                               << "event_type=" << event->type()
                               << "event_window_id=" << xwaylandWindowId(wrapper)
                               << "event_wrapper=" << wrapper
                               << "sequence_window_id=" << xwaylandWindowId(sequenceWrapper)
                               << "sequence_wrapper=" << sequenceWrapper
                               << "seat=" << seat
                               << "client=" << sequence.client
                               << "popup_like=" << sequence.popupLike
                               << "pending_unmap_release=" << sequence.pendingUnmapRelease
                               << "pressed_buttons_before=" << oldPressedButtons
                               << "pressed_buttons_after=" << sequence.pressedButtons;
        return;
    }

    auto sequenceIt = m_xwaylandPointerButtonSequences.find(seat);
    if (sequenceIt == m_xwaylandPointerButtonSequences.end()) {
        qCDebug(lcTlXwayland) << "[XWL_POINTER_SEQUENCE] Ignore XWayland pointer release without active sequence:"
                               << "reason=" << reason
                               << "event_window_id=" << xwaylandWindowId(wrapper)
                               << "event_wrapper=" << wrapper
                               << "seat=" << seat;
        return;
    }

    const int oldPressedButtons = sequenceIt->pressedButtons;
    if (sequenceIt->pressedButtons > 0)
        sequenceIt->pressedButtons -= 1;

    auto *sequenceWrapper = qobject_cast<SurfaceWrapper *>(sequenceIt->wrapper.data());
    const int newPressedButtons = sequenceIt->pressedButtons;
    qCDebug(lcTlXwayland) << "[XWL_POINTER_SEQUENCE] Update XWayland pointer button sequence:"
                           << "reason=" << reason
                           << "event_type=" << event->type()
                           << "event_window_id=" << xwaylandWindowId(wrapper)
                           << "event_wrapper=" << wrapper
                           << "sequence_window_id="
                           << (sequenceWrapper ? xwaylandWindowId(sequenceWrapper) : sequenceIt->windowId)
                           << "sequence_wrapper=" << sequenceWrapper
                           << "seat=" << seat
                           << "popup_like=" << sequenceIt->popupLike
                           << "pending_unmap_release=" << sequenceIt->pendingUnmapRelease
                           << "pressed_buttons_before=" << oldPressedButtons
                           << "pressed_buttons_after=" << newPressedButtons;

    if (newPressedButtons <= 0) {
        m_xwaylandPointerButtonSequences.erase(sequenceIt);
        scheduleXWaylandPopupParentFocusRefresh(seat, "button-sequence-finished");
    }
}

void Helper::clearXWaylandPointerButtonSequence(WXWaylandSurface *surface, const char *reason)
{
    for (auto it = m_xwaylandPointerButtonSequences.begin();
         it != m_xwaylandPointerButtonSequences.end();) {
        auto *wrapper = qobject_cast<SurfaceWrapper *>(it->wrapper.data());
        auto *xwaylandSurface =
            wrapper ? qobject_cast<WXWaylandSurface *>(wrapper->shellSurface()) : nullptr;
        if (xwaylandSurface != surface) {
            ++it;
            continue;
        }

        if (it->popupLike && it->pressedButtons > 0) {
            it->pendingUnmapRelease = true;
            it->windowId = xwaylandWindowId(surface);
            qCDebug(lcTlXwayland) << "[XWL_POINTER_SEQUENCE_PENDING_RELEASE] Keep XWayland popup button sequence until release:"
                                   << "reason=" << reason
                                   << "window_id=" << it->windowId
                                   << "surface=" << surface
                                   << "wrapper=" << wrapper
                                   << "seat=" << it.key()
                                   << "popup_like=" << it->popupLike
                                   << "pressed_buttons=" << it->pressedButtons;
            ++it;
            continue;
        }

        qCDebug(lcTlXwayland) << "[XWL_POINTER_SEQUENCE] Clear XWayland pointer button sequence:"
                              << "reason=" << reason
                              << "window_id=" << xwaylandWindowId(surface)
                              << "surface=" << surface
                              << "wrapper=" << wrapper
                              << "seat=" << it.key()
                              << "popup_like=" << it->popupLike
                              << "pending_unmap_release=" << it->pendingUnmapRelease
                              << "pressed_buttons=" << it->pressedButtons;
        it = m_xwaylandPointerButtonSequences.erase(it);
    }
}

bool Helper::redirectXWaylandPopupBlockedRelease(SurfaceWrapper *popup,
                                                 SurfaceWrapper *sequenceWrapper,
                                                 WSeat *seat,
                                                 QInputEvent *event,
                                                 const char *reason)
{
    if (!popup || !sequenceWrapper || !seat || !event
        || event->type() != QEvent::MouseButtonRelease) {
        return false;
    }
    if (!sequenceWrapper->surface() || !sequenceWrapper->surface()->mapped()
        || !sequenceWrapper->surfaceItem() || !sequenceWrapper->surfaceItem()->eventItem()) {
        return false;
    }

    QPointF sequenceLocalPos;
    popupPointerLocalPosition(sequenceWrapper, seat, event, &sequenceLocalPos, nullptr);

    QScopedValueRollback<bool> redirectGuard(m_redirectingXWaylandPopupPointerEvent, true);
    const bool delivered = seat->redirectPointerEvent(sequenceWrapper->surface(),
                                                      sequenceWrapper->surfaceItem()->eventItem(),
                                                      event,
                                                      sequenceLocalPos);
    qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_RELEASE_REDIRECT] Redirect blocked XWayland popup release to button sequence owner:"
                           << "delivered=" << delivered
                           << "reason=" << reason
                           << "event_type=" << event->type()
                           << "popup_window_id=" << xwaylandWindowId(popup)
                           << "popup=" << popup
                           << "popup_parentWrapper=" << xwaylandPopupTransientParentWrapper(popup, seat)
                           << "sequence_window_id=" << xwaylandWindowId(sequenceWrapper)
                           << "sequence_wrapper=" << sequenceWrapper
                           << "seat=" << seat
                           << "local_pos=" << sequenceLocalPos
                           << "scene_pos=" << pointerEventScenePosition(event)
                           << "popup_geometry=" << popup->geometry();

    if (delivered) {
        updateXWaylandPointerButtonSequence(sequenceWrapper, seat, event, reason);
        armXWaylandPopupOpeningPointerGuard(popup, seat, event, reason);
    }

    return delivered;
}

bool Helper::finishXWaylandPendingPointerButtonRelease(WSeat *seat,
                                                       QInputEvent *event,
                                                       const char *reason)
{
    if (!seat || !event || event->type() != QEvent::MouseButtonRelease)
        return false;

    auto sequenceIt = m_xwaylandPointerButtonSequences.find(seat);
    if (sequenceIt == m_xwaylandPointerButtonSequences.end()
        || sequenceIt->pressedButtons <= 0
        || !xwaylandPointerButtonSequenceNeedsRecovery(seat)) {
        return false;
    }

    auto *sequenceWrapper = qobject_cast<SurfaceWrapper *>(sequenceIt->wrapper.data());
    if (sequenceWrapper && sequenceWrapper->surface() && sequenceWrapper->surface()->mapped()
        && sequenceWrapper->surfaceItem() && sequenceWrapper->surfaceItem()->eventItem()) {
        QPointF localPos;
        popupPointerLocalPosition(sequenceWrapper, seat, event, &localPos, nullptr);

        QScopedValueRollback<bool> redirectGuard(m_redirectingXWaylandPopupPointerEvent, true);
        const bool delivered = seat->redirectPointerEvent(sequenceWrapper->surface(),
                                                          sequenceWrapper->surfaceItem()->eventItem(),
                                                          event,
                                                          localPos);
        qCDebug(lcTlXwayland) << "[XWL_POINTER_SEQUENCE_PENDING_RELEASE] Redirect pending XWayland popup release:"
                               << "delivered=" << delivered
                               << "reason=" << reason
                               << "event_type=" << event->type()
                               << "sequence_window_id=" << xwaylandWindowId(sequenceWrapper)
                               << "sequence_wrapper=" << sequenceWrapper
                               << "seat=" << seat
                               << "local_pos=" << localPos
                               << "scene_pos=" << pointerEventScenePosition(event);
        if (delivered) {
            updateXWaylandPointerButtonSequence(sequenceWrapper,
                                                seat,
                                                event,
                                                "pending-unmap-release-delivered");
            return true;
        }
    }

    return recoverXWaylandPointerButtonSequenceRelease(seat, event, sequenceWrapper, reason);
}

bool Helper::clearXWaylandPendingPointerButtonSequenceForPress(WSeat *seat,
                                                               SurfaceWrapper *eventWrapper,
                                                               QInputEvent *event,
                                                               const char *reason)
{
    if (!seat || !event || event->type() != QEvent::MouseButtonPress)
        return false;

    auto sequenceIt = m_xwaylandPointerButtonSequences.find(seat);
    if (sequenceIt == m_xwaylandPointerButtonSequences.end()
        || sequenceIt->pressedButtons <= 0
        || !xwaylandPointerButtonSequenceNeedsRecovery(seat)) {
        return false;
    }

    auto *sequenceWrapper =
        qobject_cast<SurfaceWrapper *>(sequenceIt->wrapper.data());
    const bool sequenceMapped =
        sequenceWrapper && sequenceWrapper->surface() && sequenceWrapper->surface()->mapped();
    const auto *mouseEvent = static_cast<QMouseEvent *>(event);
    qCDebug(lcTlXwayland) << "[XWL_POINTER_SEQUENCE_PENDING_RESET] Clear stale XWayland pending pointer sequence before fresh press:"
                           << "reason=" << reason
                           << "event_type=" << event->type()
                           << "event_window_id=" << xwaylandWindowId(eventWrapper)
                           << "event_wrapper=" << eventWrapper
                           << "sequence_window_id="
                           << (sequenceWrapper ? xwaylandWindowId(sequenceWrapper)
                                               : sequenceIt->windowId)
                           << "sequence_wrapper=" << sequenceWrapper
                           << "seat=" << seat
                           << "popup_like=" << sequenceIt->popupLike
                           << "pending_unmap_release="
                           << sequenceIt->pendingUnmapRelease
                           << "sequence_mapped=" << sequenceMapped
                           << "pressed_buttons=" << sequenceIt->pressedButtons
                           << "button=" << mouseEvent->button()
                           << "buttons=" << mouseEvent->buttons()
                           << "scene_pos=" << pointerEventScenePosition(event)
                           << "event_wrapper_geometry="
                           << (eventWrapper ? eventWrapper->geometry() : QRectF());

    m_xwaylandPointerButtonSequences.erase(sequenceIt);
    return true;
}

bool Helper::filterXWaylandPendingPointerButtonSequence(WSeat *seat,
                                                        SurfaceWrapper *eventWrapper,
                                                        QInputEvent *event,
                                                        const char *reason)
{
    if (!seat || !event)
        return false;

    if (finishXWaylandPendingPointerButtonRelease(seat, event, reason))
        return true;

    if (event->type() != QEvent::MouseButtonPress)
        return false;

    clearXWaylandPendingPointerButtonSequenceForPress(seat,
                                                      eventWrapper,
                                                      event,
                                                      reason);
    return false;
}

bool Helper::recoverXWaylandPointerButtonSequenceRelease(WSeat *seat,
                                                         QInputEvent *event,
                                                         SurfaceWrapper *eventWrapper,
                                                         const char *reason)
{
    if (!seat || !event || event->type() != QEvent::MouseButtonRelease)
        return false;

    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    const auto sequenceIt = m_xwaylandPointerButtonSequences.find(seat);
    auto *sequenceWrapper =
        sequenceIt != m_xwaylandPointerButtonSequences.end()
        ? qobject_cast<SurfaceWrapper *>(sequenceIt->wrapper.data())
        : nullptr;
    auto *parentWrapper =
        sequenceIt != m_xwaylandPointerButtonSequences.end()
        ? qobject_cast<SurfaceWrapper *>(sequenceIt->parentWrapper.data())
        : nullptr;
    auto *sequenceClient =
        sequenceIt != m_xwaylandPointerButtonSequences.end()
        ? sequenceIt->client.data()
        : nullptr;
    const uint32_t sequenceWindowId =
        sequenceIt != m_xwaylandPointerButtonSequences.end() ? sequenceIt->windowId : 0;
    const bool sequencePopupLike =
        sequenceIt != m_xwaylandPointerButtonSequences.end() && sequenceIt->popupLike;
    const bool sequencePendingUnmapRelease =
        sequenceIt != m_xwaylandPointerButtonSequences.end()
        && sequenceIt->pendingUnmapRelease;
    const int sequencePressedButtons =
        sequenceIt != m_xwaylandPointerButtonSequences.end() ? sequenceIt->pressedButtons : 0;
    const bool sequenceMapped =
        sequenceWrapper && sequenceWrapper->surface() && sequenceWrapper->surface()->mapped();

    SurfaceWrapper *recoveryTarget = nullptr;
    if (parentWrapper && parentWrapper->surface() && parentWrapper->surface()->mapped()
        && parentWrapper->surfaceItem() && parentWrapper->surfaceItem()->eventItem()
        && parentWrapper->surface()->waylandClient() == sequenceClient) {
        recoveryTarget = parentWrapper;
    } else if (eventWrapper && eventWrapper->surface() && eventWrapper->surface()->mapped()
               && eventWrapper->surfaceItem() && eventWrapper->surfaceItem()->eventItem()
               && eventWrapper->surface()->waylandClient() == sequenceClient) {
        recoveryTarget = eventWrapper;
    }

    QPointF localPos;
    if (recoveryTarget)
        popupPointerLocalPosition(recoveryTarget, seat, event, &localPos, nullptr);

    const bool recovered = sequenceClient
        && seat->recoverPointerButtonRelease(recoveryTarget ? recoveryTarget->surface() : nullptr,
                                             recoveryTarget && recoveryTarget->surfaceItem()
                                                 ? recoveryTarget->surfaceItem()->eventItem()
                                                 : nullptr,
                                             localPos,
                                             mouseEvent,
                                             sequenceClient);
    qCDebug(lcTlXwayland) << "[XWL_POINTER_SEQUENCE_RELEASE_RECOVERY] Recover XWayland pointer release for pending sequence:"
                           << "recovered=" << recovered
                           << "reason=" << reason
                           << "event_type=" << event->type()
                           << "event_window_id=" << xwaylandWindowId(eventWrapper)
                           << "event_wrapper=" << eventWrapper
                           << "sequence_window_id=" << sequenceWindowId
                           << "sequence_wrapper=" << sequenceWrapper
                           << "parent_wrapper=" << parentWrapper
                           << "recovery_target=" << recoveryTarget
                           << "sequence_client=" << sequenceClient
                           << "pointer_focus_surface=" << seat->pointerFocusSurface()
                           << "pointer_focus_client="
                           << (seat->pointerFocusSurface()
                                   ? seat->pointerFocusSurface()->waylandClient()
                                   : nullptr)
                           << "popup_like=" << sequencePopupLike
                           << "pending_unmap_release=" << sequencePendingUnmapRelease
                           << "sequence_mapped=" << sequenceMapped
                           << "pressed_buttons=" << sequencePressedButtons
                           << "seat=" << seat
                           << "local_pos=" << localPos
                           << "scene_pos=" << pointerEventScenePosition(event);

    if (!recovered)
        return false;

    updateXWaylandPointerButtonSequence(eventWrapper,
                                        seat,
                                        event,
                                        "pending-unmap-release-recovered");
    event->accept();
    return true;
}

void Helper::armXWaylandPopupOpeningPointerGuard(SurfaceWrapper *wrapper,
                                                 WSeat *seat,
                                                 const QInputEvent *event,
                                                 const char *reason)
{
    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient() || !seat || !event
        || !event->isSinglePointEvent()) {
        return;
    }

    auto &guard = m_xwaylandPopupOpeningPointerGuards[seat];
    guard.wrapper = wrapper;
    guard.seat = seat;
    guard.scenePos = pointerEventScenePosition(event);

    qCDebug(lcTlXwayland) << "[XWL_POPUP_OPENING_GUARD] Arm XWayland popup opening pointer guard:"
                           << "reason=" << reason
                           << "window_id=" << xwaylandWindowId(wrapper)
                           << "wrapper=" << wrapper
                           << "seat=" << seat
                           << "scene_pos=" << guard.scenePos
                           << "geometry=" << wrapper->geometry();
}

bool Helper::filterXWaylandPopupOpeningPointerEvent(SurfaceWrapper *wrapper,
                                                    WSeat *seat,
                                                    QInputEvent *event,
                                                    const char *reason)
{
    if (!seat || !event)
        return false;

    auto guardIt = m_xwaylandPopupOpeningPointerGuards.find(seat);
    if (guardIt == m_xwaylandPopupOpeningPointerGuards.end())
        return false;

    auto *guardWrapper = qobject_cast<SurfaceWrapper *>(guardIt->wrapper.data());
    if (!guardWrapper || !guardWrapper->surface() || !guardWrapper->surface()->mapped()) {
        qCDebug(lcTlXwayland) << "[XWL_POPUP_OPENING_GUARD] Clear stale XWayland popup opening pointer guard:"
                               << "reason=" << reason
                               << "guard_window_id=" << xwaylandWindowId(guardWrapper)
                               << "guard_wrapper=" << guardWrapper
                               << "event_window_id=" << xwaylandWindowId(wrapper)
                               << "event_wrapper=" << wrapper
                               << "seat=" << seat;
        m_xwaylandPopupOpeningPointerGuards.erase(guardIt);
        return false;
    }

    if (guardWrapper != wrapper)
        return false;

    if (!isXWaylandPopupPointerEnterOrMove(event)) {
        qCDebug(lcTlXwayland) << "[XWL_POPUP_OPENING_GUARD] Clear XWayland popup opening pointer guard:"
                               << "reason= intentional-pointer-event"
                               << "event_reason=" << reason
                               << "event_type=" << event->type()
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "seat=" << seat;
        m_xwaylandPopupOpeningPointerGuards.erase(guardIt);
        return false;
    }

    const QPointF scenePos = pointerEventScenePosition(event);
    const qreal distance = (scenePos - guardIt->scenePos).manhattanLength();
    const QStyleHints *styleHints = QGuiApplication::styleHints();
    const int threshold = styleHints ? styleHints->startDragDistance() : 10;
    if (distance > threshold) {
        qCDebug(lcTlXwayland) << "[XWL_POPUP_OPENING_GUARD] Clear XWayland popup opening pointer guard:"
                               << "reason= pointer-moved"
                               << "event_reason=" << reason
                               << "event_type=" << event->type()
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "seat=" << seat
                               << "distance=" << distance
                               << "threshold=" << threshold
                               << "scene_pos=" << scenePos
                               << "guard_scene_pos=" << guardIt->scenePos;
        m_xwaylandPopupOpeningPointerGuards.erase(guardIt);
        return false;
    }

    qCDebug(lcTlXwayland) << "[XWL_POPUP_OPENING_GUARD] Suppress XWayland popup opening pointer event:"
                           << "reason=" << reason
                           << "event_type=" << event->type()
                           << "window_id=" << xwaylandWindowId(wrapper)
                           << "wrapper=" << wrapper
                           << "seat=" << seat
                           << "distance=" << distance
                           << "threshold=" << threshold
                           << "scene_pos=" << scenePos
                           << "guard_scene_pos=" << guardIt->scenePos
                           << "geometry=" << wrapper->geometry();
    event->accept();
    return true;
}

void Helper::clearXWaylandPopupOpeningPointerGuard(WXWaylandSurface *surface, const char *reason)
{
    for (auto it = m_xwaylandPopupOpeningPointerGuards.begin();
         it != m_xwaylandPopupOpeningPointerGuards.end();) {
        auto *wrapper = qobject_cast<SurfaceWrapper *>(it->wrapper.data());
        auto *xwaylandSurface =
            wrapper ? qobject_cast<WXWaylandSurface *>(wrapper->shellSurface()) : nullptr;
        if (xwaylandSurface != surface) {
            ++it;
            continue;
        }

        qCDebug(lcTlXwayland) << "[XWL_POPUP_OPENING_GUARD] Clear XWayland popup opening pointer guard:"
                               << "reason=" << reason
                               << "window_id=" << xwaylandWindowId(surface)
                               << "surface=" << surface
                               << "wrapper=" << wrapper
                               << "seat=" << it.key()
                               << "scene_pos=" << it->scenePos;
        it = m_xwaylandPopupOpeningPointerGuards.erase(it);
    }
}

bool Helper::redirectXWaylandPopupPointerEvent(WSeat *seat,
                                               SurfaceWrapper *eventTarget,
                                               QObject *eventObject,
                                               QInputEvent *event)
{
    if (!seat || !event || m_redirectingXWaylandPopupPointerEvent
        || !isXWaylandPopupPointerEvent(event)) {
        return false;
    }

    if (eventTarget && eventTarget->isXWaylandPopupLikeTransient())
        return false;

    QPointF targetLocalPos;
    auto *targetPopup =
        findXWaylandPopupPointerTarget(seat, eventTarget, event, &targetLocalPos);
    if (!targetPopup || !targetPopup->surface() || !targetPopup->surfaceItem()
        || !targetPopup->surfaceItem()->eventItem()) {
        return false;
    }

    const bool buttonSequenceBelongsToPopup =
        xwaylandPointerButtonSequenceBelongsTo(seat, targetPopup);
    const bool buttonSequenceBlocksPopup =
        xwaylandPointerButtonSequenceBlocksPopup(targetPopup, seat);
    const bool allowRelatedPopupMove =
        buttonSequenceBlocksPopup
        && xwaylandPointerButtonSequenceAllowsRelatedPopupMove(targetPopup, seat, event);
    if ((buttonSequenceBlocksPopup && !allowRelatedPopupMove)
        || (xwaylandPopupPointerEventNeedsButtonSequence(event)
            && !buttonSequenceBelongsToPopup)) {
        auto *sequenceWrapper = activeXWaylandPointerButtonSequenceWrapper(seat);
        const auto sequenceIt = m_xwaylandPointerButtonSequences.constFind(seat);
        const int sequencePressedButtons =
            sequenceIt != m_xwaylandPointerButtonSequences.constEnd()
            ? sequenceIt->pressedButtons
            : 0;
        qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_REDIRECT_SKIP] Skip XWayland popup pointer redirect:"
                               << "reason="
                               << (buttonSequenceBlocksPopup
                                       ? "active-button-sequence-on-other-surface"
                                       : "missing-popup-button-sequence")
                               << "event_type=" << event->type()
                               << "popup_window_id=" << xwaylandWindowId(targetPopup)
                               << "popup=" << targetPopup
                               << "eventTarget=" << eventTarget
                               << "sequence_window_id=" << xwaylandWindowId(sequenceWrapper)
                               << "sequence_wrapper=" << sequenceWrapper
                               << "sequence_pressed_buttons=" << sequencePressedButtons
                               << "seat=" << seat
                               << "eventObject=" << eventObject
                               << "local_pos=" << targetLocalPos
                               << "scene_pos=" << pointerEventScenePosition(event)
                               << "popup_geometry=" << targetPopup->geometry();
        if (buttonSequenceBlocksPopup && sequenceWrapper
            && event->type() == QEvent::MouseButtonRelease) {
            if (redirectXWaylandPopupBlockedRelease(targetPopup,
                                                    sequenceWrapper,
                                                    seat,
                                                    event,
                                                    "redirect-skip-sequence-release")) {
                return true;
            }
            if (recoverXWaylandPointerButtonSequenceRelease(seat,
                                                            event,
                                                            sequenceWrapper,
                                                            "redirect-skip-sequence-release-fallback")) {
                return true;
            }
        }
        return false;
    }

    if (allowRelatedPopupMove) {
        auto *sequenceWrapper = activeXWaylandPointerButtonSequenceWrapper(seat);
        const auto sequenceIt = m_xwaylandPointerButtonSequences.constFind(seat);
        const int sequencePressedButtons =
            sequenceIt != m_xwaylandPointerButtonSequences.constEnd()
            ? sequenceIt->pressedButtons
            : 0;
        qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_RELATED_MOVE] Allow related XWayland popup pointer move during active sequence:"
                               << "event_type=" << event->type()
                               << "popup_window_id=" << xwaylandWindowId(targetPopup)
                               << "popup=" << targetPopup
                               << "eventTarget=" << eventTarget
                               << "sequence_window_id=" << xwaylandWindowId(sequenceWrapper)
                               << "sequence_wrapper=" << sequenceWrapper
                               << "sequence_pressed_buttons=" << sequencePressedButtons
                               << "seat=" << seat
                               << "eventObject=" << eventObject
                               << "local_pos=" << targetLocalPos
                               << "scene_pos=" << pointerEventScenePosition(event)
                               << "popup_geometry=" << targetPopup->geometry();
    }

    if (filterXWaylandPopupOpeningPointerEvent(targetPopup,
                                               seat,
                                               event,
                                               "redirect-pointer-event")) {
        return true;
    }

    if (!xwaylandPopupKeepsParentFocusOnPointer(targetPopup, seat)
        && event->isSinglePointEvent()
        && (static_cast<QSinglePointEvent *>(event)->isBeginEvent()
            || event->type() == QEvent::HoverEnter
            || event->type() == QEvent::HoverMove)) {
        offerXWaylandPopupDeferredTakeFocus(xwaylandSurfaceForWrapper(targetPopup),
                                            targetPopup,
                                            Qt::MouseFocusReason,
                                            seat,
                                            "redirect-before-pointer-delivery");
    }

    setXWaylandPopupPointerOwner(targetPopup, seat, "redirect-pointer-event");

    QScopedValueRollback<bool> redirectGuard(m_redirectingXWaylandPopupPointerEvent, true);
    const bool delivered = seat->redirectPointerEvent(targetPopup->surface(),
                                                      targetPopup->surfaceItem()->eventItem(),
                                                      event,
                                                      targetLocalPos);
    qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_REDIRECT] Redirect XWayland popup pointer event:"
                           << "delivered=" << delivered
                           << "event_type=" << event->type()
                           << "popup_window_id=" << xwaylandWindowId(targetPopup)
                           << "popup=" << targetPopup
                           << "eventTarget=" << eventTarget
                           << "seat=" << seat
                           << "eventObject=" << eventObject
                           << "local_pos=" << targetLocalPos
                           << "scene_pos=" << pointerEventScenePosition(event)
                           << "popup_geometry=" << targetPopup->geometry();

    if (delivered) {
        const bool sequenceBelongsBeforeUpdate =
            xwaylandPointerButtonSequenceBelongsTo(seat, targetPopup);
        updateXWaylandPopupPointerOwnerForEvent(targetPopup, seat, event, "redirect-delivered");
        updateXWaylandPointerButtonSequence(targetPopup, seat, event, "redirect-delivered");

        if (event->isSinglePointEvent()
            && static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
            updateSurfaceSeatInteraction(targetPopup, seat);
            offerXWaylandPopupTransientFocus(targetPopup, Qt::MouseFocusReason, seat);
        } else if (event->type() == QEvent::MouseButtonRelease) {
            commitDeferredXWaylandPopupTransientFocus(targetPopup,
                                                      Qt::MouseFocusReason,
                                                      seat,
                                                      sequenceBelongsBeforeUpdate,
                                                      "redirect-release-delivered");
        }
    }

    return delivered;
}

bool Helper::redirectDirectXWaylandPopupPointerEvent(SurfaceWrapper *wrapper,
                                                     WSeat *seat,
                                                     QObject *eventObject,
                                                     QInputEvent *event)
{
    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient() || !seat || !event
        || m_redirectingXWaylandPopupPointerEvent || !isXWaylandPopupPointerEvent(event)) {
        return false;
    }

    if (!wrapper->surface() || !wrapper->surfaceItem()
        || !wrapper->surfaceItem()->eventItem()) {
        return false;
    }
    QObject *targetEventObject = eventObject ? eventObject : wrapper->surfaceItem()->eventItem();
    const bool pointerFocusMatchesBefore =
        seat->pointerFocusMatches(wrapper->surface(), targetEventObject);

    if (filterXWaylandPopupOpeningPointerEvent(wrapper,
                                               seat,
                                               event,
                                               "direct-popup-pointer-event")) {
        return true;
    }

    QPointF localPos;
    bool contains = false;
    bool usedX11RequestPosition = false;
    bool usedX11EffectiveRequestGeometry = false;
    bool usedGlobalItemPosition = false;
    XWaylandPopupGlobalHit globalHit;
    if (!popupPointerLocalPosition(wrapper,
                                   seat,
                                   event,
                                   &localPos,
                                   &contains,
                                   &usedX11RequestPosition,
                                   X11GlobalRequestFallbackReason::None,
                                   nullptr,
                                   &usedX11EffectiveRequestGeometry)) {
        const auto *pointEvent =
            event->isSinglePointEvent() ? static_cast<QSinglePointEvent *>(event) : nullptr;
        if (pointEvent)
            localPos = pointEvent->position();
    }
    if (!contains) {
        globalHit = xwaylandPopupGlobalHitTest(wrapper, seat, event);
        if (globalHit.contains) {
            localPos = globalHit.localPos;
            contains = true;
            usedGlobalItemPosition = true;
        }
    } else {
        globalHit = xwaylandPopupGlobalHitTest(wrapper, seat, event);
    }

    const bool sequenceBelongsToWrapper =
        xwaylandPointerButtonSequenceBelongsTo(seat, wrapper);
    const bool relatedSequenceMove =
        xwaylandPointerButtonSequenceAllowsRelatedPopupMove(wrapper, seat, event);
    const bool pointerHasGrab = seat->pointerHasGrab();
    if (!contains && !sequenceBelongsToWrapper && !relatedSequenceMove && !pointerHasGrab) {
        qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_DIRECT_SKIP] Skip direct XWayland popup pointer redirect:"
                               << "reason= outside-without-pointer-grab"
                               << "event_type=" << event->type()
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "parentWrapper="
                               << xwaylandPopupTransientParentWrapper(wrapper, seat)
                               << "seat=" << seat
                               << "eventObject=" << targetEventObject
                               << "local_pos=" << localPos
                               << "contains=" << contains
                               << "used_global_item_position=" << usedGlobalItemPosition
                               << "cursor_pos=" << globalHit.cursorPos
                               << "global_item_local_pos=" << globalHit.localPos
                               << "global_item_contains=" << globalHit.itemContains
                               << "global_input_region_contains="
                               << globalHit.inputRegionContains
                               << "global_item_geometry=" << globalHit.itemGlobalGeometry
                               << "sequence_belongs=" << sequenceBelongsToWrapper
                               << "related_sequence_move=" << relatedSequenceMove
                               << "pointer_has_grab=" << pointerHasGrab
                               << "scene_pos=" << pointerEventScenePosition(event)
                               << "geometry=" << wrapper->geometry();
        return false;
    }

    if (contains && !xwaylandPopupKeepsParentFocusOnPointer(wrapper, seat)
        && event->isSinglePointEvent()
        && (static_cast<QSinglePointEvent *>(event)->isBeginEvent()
            || event->type() == QEvent::HoverEnter
            || event->type() == QEvent::HoverMove)) {
        offerXWaylandPopupDeferredTakeFocus(xwaylandSurfaceForWrapper(wrapper),
                                            wrapper,
                                            Qt::MouseFocusReason,
                                            seat,
                                            "direct-before-pointer-delivery");
    }

    setXWaylandPopupPointerOwner(wrapper, seat, "direct-popup-pointer-event");

    QScopedValueRollback<bool> redirectGuard(m_redirectingXWaylandPopupPointerEvent, true);
    const bool delivered = seat->redirectPointerEvent(wrapper->surface(),
                                                      targetEventObject,
                                                      event,
                                                      localPos);
    qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_DIRECT_REDIRECT] Redirect direct XWayland popup pointer event:"
                           << "delivered=" << delivered
                           << "event_type=" << event->type()
                           << "window_id=" << xwaylandWindowId(wrapper)
                           << "wrapper=" << wrapper
                           << "parentWrapper=" << xwaylandPopupTransientParentWrapper(wrapper, seat)
                           << "seat=" << seat
                           << "eventObject=" << targetEventObject
                           << "local_pos=" << localPos
                           << "contains=" << contains
                           << "used_global_item_position=" << usedGlobalItemPosition
                           << "cursor_pos=" << globalHit.cursorPos
                           << "global_item_local_pos=" << globalHit.localPos
                           << "global_item_contains=" << globalHit.itemContains
                           << "global_input_region_contains="
                           << globalHit.inputRegionContains
                           << "global_item_geometry=" << globalHit.itemGlobalGeometry
                           << "used_x11_request_position=" << usedX11RequestPosition
                           << "used_x11_effective_request_geometry="
                           << usedX11EffectiveRequestGeometry
                           << "dispatch_mode= recovery"
                           << "pointer_focus_matches_before="
                           << pointerFocusMatchesBefore
                           << "scene_pos=" << pointerEventScenePosition(event)
                           << "geometry=" << wrapper->geometry();

    if (!delivered)
        return false;

    updateXWaylandPopupPointerOwnerForEvent(wrapper,
                                            seat,
                                            event,
                                            "direct-redirect-delivered");
    updateXWaylandPointerButtonSequence(wrapper, seat, event, "direct-redirect-delivered");

    if (event->isSinglePointEvent()
        && static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
        updateSurfaceSeatInteraction(wrapper, seat);
        offerXWaylandPopupTransientFocus(wrapper, Qt::MouseFocusReason, seat);
        qCDebug(lcTlXwayland) << "[XWL_POPUP_MOUSE_ACTIVATE_SKIP] Skip XWayland popup activation after redirected pointer begin:"
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "parentWrapper="
                               << xwaylandPopupTransientParentWrapper(wrapper, seat)
                               << "seat=" << seat
                               << "event_type=" << event->type()
                               << "geometry=" << wrapper->geometry();
    } else if (event->type() == QEvent::MouseButtonRelease) {
        commitDeferredXWaylandPopupTransientFocus(wrapper,
                                                  Qt::MouseFocusReason,
                                                  seat,
                                                  sequenceBelongsToWrapper,
                                                  "direct-release-delivered");
    }

    return true;
}

bool Helper::deferXWaylandPopupTransientFocus(SurfaceWrapper *wrapper,
                                              Qt::FocusReason reason,
                                              WSeat *seat,
                                              const char *source)
{
    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient() || !seat)
        return false;

    auto *xwaylandSurface = xwaylandSurfaceForWrapper(wrapper);
    if (!xwaylandSurface)
        return false;

    if (reason == Qt::MouseFocusReason && xwaylandPopupKeepsParentFocusOnPointer(wrapper, seat)) {
        qCDebug(lcTlXwayland) << "[XWL_POPUP_FOCUS_DECISION] XWayland popup focus decision:"
                               << "action=" << "skip-defer-pointer-focus"
                               << "why=" << "keep-parent-focus"
                               << "source=" << source
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "parentWrapper="
                               << xwaylandPopupTransientParentWrapper(wrapper, seat)
                               << "seat=" << seat
                               << "reason=" << reason
                               << "input_model=" << xwaylandInputModelName(xwaylandSurface->inputModel())
                               << "window_types=" << xwaylandSurface->windowTypes()
                               << "bypass_manager=" << xwaylandSurface->isBypassManager()
                               << "geometry=" << wrapper->geometry();
        return false;
    }

    XWaylandDeferredPopupFocus deferred;
    deferred.wrapper = wrapper;
    deferred.parentWrapper = xwaylandPopupTransientParentWrapper(wrapper, seat);
    deferred.seat = seat;
    deferred.reason = reason;
    deferred.windowId = xwaylandWindowId(wrapper);
    m_deferredXWaylandPopupFocuses.insert(seat, deferred);

    qCDebug(lcTlXwayland) << "[XWL_POPUP_FOCUS_DECISION] XWayland popup focus decision:"
                           << "action=" << "defer-pointer-focus"
                           << "source=" << source
                           << "window_id=" << deferred.windowId
                           << "wrapper=" << wrapper
                           << "parentWrapper="
                           << qobject_cast<SurfaceWrapper *>(deferred.parentWrapper.data())
                           << "seat=" << seat
                           << "reason=" << reason
                           << "input_model=" << xwaylandInputModelName(xwaylandSurface->inputModel())
                           << "supports_wm_take_focus=" << xwaylandSurface->supportsWmTakeFocus()
                           << "geometry=" << wrapper->geometry();
    return true;
}

void Helper::commitDeferredXWaylandPopupTransientFocus(SurfaceWrapper *wrapper,
                                                       Qt::FocusReason reason,
                                                       WSeat *seat,
                                                       bool sequenceBelongsBeforeUpdate,
                                                       const char *source)
{
    if (!seat)
        return;

    auto deferredIt = m_deferredXWaylandPopupFocuses.find(seat);
    if (deferredIt == m_deferredXWaylandPopupFocuses.end())
        return;

    auto *deferredWrapper = qobject_cast<SurfaceWrapper *>(deferredIt->wrapper.data());
    auto *deferredParentWrapper =
        qobject_cast<SurfaceWrapper *>(deferredIt->parentWrapper.data());
    auto *xwaylandSurface = xwaylandSurfaceForWrapper(deferredWrapper);
    const bool sameWrapper = wrapper && deferredWrapper == wrapper;
    const bool mapped = deferredWrapper && deferredWrapper->surface()
        && deferredWrapper->surface()->mapped();
    const bool hasCapabilities = deferredWrapper && deferredWrapper->hasActiveCapability()
        && deferredWrapper->hasFocusCapability();
    const bool ownerMatches = activeXWaylandPopupPointerOwner(seat) == deferredWrapper;
    const bool alreadyFocused = seat && deferredWrapper && deferredWrapper->surface()
        && seat->keyboardFocusSurface() == deferredWrapper->surface();

    auto logDecision = [&](const char *action, const char *why) {
        qCDebug(lcTlXwayland) << "[XWL_POPUP_FOCUS_DECISION] XWayland popup focus decision:"
                               << "action=" << action
                               << "why=" << why
                               << "source=" << source
                               << "event_reason=" << reason
                               << "deferred_reason=" << deferredIt->reason
                               << "window_id="
                               << (deferredWrapper ? xwaylandWindowId(deferredWrapper)
                                                   : deferredIt->windowId)
                               << "wrapper=" << deferredWrapper
                               << "event_wrapper=" << wrapper
                               << "parentWrapper=" << deferredParentWrapper
                               << "seat=" << seat
                               << "same_wrapper=" << sameWrapper
                               << "mapped=" << mapped
                               << "has_capabilities=" << hasCapabilities
                               << "owner_matches=" << ownerMatches
                               << "sequence_belongs_before_update="
                               << sequenceBelongsBeforeUpdate
                               << "already_focused=" << alreadyFocused
                               << "input_model="
                               << xwaylandInputModelName(xwaylandSurface
                                                             ? xwaylandSurface->inputModel()
                                                             : WXWaylandSurface::InputModelNone)
                               << "geometry="
                               << (deferredWrapper ? deferredWrapper->geometry() : QRectF());
    };

    const char *skipReason = nullptr;
    if (!sameWrapper)
        skipReason = "different-wrapper";
    else if (!xwaylandSurface)
        skipReason = "missing-xwayland-surface";
    else if (!mapped)
        skipReason = "unmapped";
    else if (!hasCapabilities)
        skipReason = "missing-capability";
    else if (reason == Qt::MouseFocusReason
             && xwaylandPopupKeepsParentFocusOnPointer(deferredWrapper, seat))
        skipReason = "keep-parent-focus";
    else if (!sequenceBelongsBeforeUpdate)
        skipReason = "sequence-not-owned-by-popup";
    else if (!ownerMatches)
        skipReason = "pointer-owner-changed";
    else if (alreadyFocused)
        skipReason = "already-focused";

    if (skipReason) {
        logDecision("skip-deferred-focus", skipReason);
        m_deferredXWaylandPopupFocuses.erase(deferredIt);
        return;
    }

    const Qt::FocusReason deferredReason = deferredIt->reason;
    logDecision("commit-deferred-focus", "release-delivered");
    m_deferredXWaylandPopupFocuses.erase(deferredIt);
    commitXWaylandPopupTransientFocus(xwaylandSurface,
                                      deferredWrapper,
                                      deferredReason,
                                      seat,
                                      source,
                                      true);
}

void Helper::clearDeferredXWaylandPopupTransientFocus(WXWaylandSurface *surface,
                                                      const char *reason)
{
    for (auto it = m_deferredXWaylandPopupFocuses.begin();
         it != m_deferredXWaylandPopupFocuses.end();) {
        auto *wrapper = qobject_cast<SurfaceWrapper *>(it->wrapper.data());
        auto *xwaylandSurface = xwaylandSurfaceForWrapper(wrapper);
        if (xwaylandSurface != surface) {
            ++it;
            continue;
        }

        qCDebug(lcTlXwayland) << "[XWL_POPUP_FOCUS_DECISION] XWayland popup focus decision:"
                               << "action=" << "clear-deferred-focus"
                               << "reason=" << reason
                               << "window_id=" << xwaylandWindowId(surface)
                               << "surface=" << surface
                               << "wrapper=" << wrapper
                               << "seat=" << it.key()
                               << "deferred_reason=" << it->reason;
        it = m_deferredXWaylandPopupFocuses.erase(it);
    }
}

bool Helper::offerXWaylandPopupTransientFocus(SurfaceWrapper *wrapper,
                                              Qt::FocusReason reason,
                                              WSeat *seat)
{
    if (!wrapper || wrapper->type() != SurfaceWrapper::Type::XWayland)
        return false;

    auto *xwaylandSurface = qobject_cast<WXWaylandSurface *>(wrapper->shellSurface());
    WSeat *targetSeat = seat;
    if (!targetSeat)
        targetSeat = m_currentEventSeat ? m_currentEventSeat : getLastInteractingSeat(wrapper);
    if (!targetSeat)
        targetSeat = m_primarySeat;

    const bool mapped = wrapper->surface() && wrapper->surface()->mapped();
    const bool supportsTakeFocus = xwaylandSurface && xwaylandSurface->supportsWmTakeFocus();
    const auto inputModel =
        xwaylandSurface ? xwaylandSurface->inputModel() : WXWaylandSurface::InputModelNone;
    auto *parentWrapper = xwaylandPopupTransientParentWrapper(wrapper, targetSeat);

    if (reason == Qt::MouseFocusReason
        && xwaylandPopupKeepsParentFocusOnPointer(wrapper, targetSeat)) {
        const bool pendingOffer = xwaylandSurface
            && m_pendingXWaylandFocusOffers.contains(xwaylandSurface);
        if (xwaylandSurface) {
            m_pendingXWaylandFocusOffers.remove(xwaylandSurface);
            clearDeferredXWaylandPopupTransientFocus(xwaylandSurface, "keep-parent-focus");
        }
        qCDebug(lcTlXwayland) << "[XWL_POPUP_FOCUS_DECISION] XWayland popup focus decision:"
                               << "action=" << "keep-parent-focus"
                               << "why=" << "nonactivating-managed-utility-pointer"
                               << "source=" << "mouse-focus-offer"
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "parentWrapper=" << parentWrapper
                               << "seat=" << targetSeat
                               << "reason=" << reason
                               << "mapped=" << mapped
                               << "has_active_capability=" << wrapper->hasActiveCapability()
                               << "has_focus_capability=" << wrapper->hasFocusCapability()
                               << "input_model=" << xwaylandInputModelName(inputModel)
                               << "window_types="
                               << (xwaylandSurface ? xwaylandSurface->windowTypes()
                                                   : WXWaylandSurface::WindowTypes())
                               << "bypass_manager="
                               << (xwaylandSurface ? xwaylandSurface->isBypassManager() : false)
                               << "pointer_grab_focus="
                               << xwaylandPopupHasPointerGrabFocus(wrapper)
                               << "supports_wm_take_focus=" << supportsTakeFocus
                               << "pending_offer=" << pendingOffer
                               << "geometry=" << wrapper->geometry();
        return true;
    }

    qCDebug(lcTlXwayland) << "[XWL_OFFER_FOCUS] Offer focus for XWayland popup-like transient:"
                           << "window_id=" << xwaylandWindowId(wrapper)
                           << "wrapper=" << wrapper
                           << "parentWrapper=" << parentWrapper
                           << "seat=" << targetSeat
                           << "reason=" << reason
                           << "mapped=" << mapped
                           << "has_active_capability=" << wrapper->hasActiveCapability()
                           << "has_focus_capability=" << wrapper->hasFocusCapability()
                           << "input_model=" << xwaylandInputModelName(inputModel)
                           << "supports_wm_take_focus=" << supportsTakeFocus
                           << "geometry=" << wrapper->geometry();

    if (!xwaylandSurface || !mapped || !wrapper->hasActiveCapability()
        || !wrapper->hasFocusCapability()) {
        qCDebug(lcTlXwayland) << "[XWL_FOCUS_ACCEPT_SKIP] Skip XWayland focus offer:"
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "surface=" << xwaylandSurface
                               << "mapped=" << mapped
                               << "has_active_capability=" << wrapper->hasActiveCapability()
                               << "has_focus_capability=" << wrapper->hasFocusCapability()
                               << "input_model=" << xwaylandInputModelName(inputModel);
        return true;
    }

    if (targetSeat && wrapper->surface()
        && targetSeat->keyboardFocusSurface() == wrapper->surface()) {
        qCDebug(lcTlXwayland) << "[XWL_FOCUS_ACCEPT_SKIP] XWayland popup focus already active:"
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "surface=" << xwaylandSurface
                               << "seat=" << targetSeat
                               << "reason=" << reason
                               << "input_model=" << xwaylandInputModelName(inputModel);
        return true;
    }

    if (reason == Qt::MouseFocusReason
        && commitXWaylandPointerGrabPopupFocus(wrapper,
                                               reason,
                                               targetSeat,
                                               "x11-pointer-grab-focus")) {
        return true;
    }

    if (xwaylandPopupUsesDeferredPointerFocus(wrapper)) {
        if (reason == Qt::MouseFocusReason) {
            offerXWaylandPopupDeferredTakeFocus(xwaylandSurface,
                                                wrapper,
                                                reason,
                                                targetSeat,
                                                "deferred-focus-fallback");
            return deferXWaylandPopupTransientFocus(wrapper,
                                                    reason,
                                                    targetSeat,
                                                    "local-input-pointer-press");
        }
        commitXWaylandPopupTransientFocus(xwaylandSurface,
                                          wrapper,
                                          reason,
                                          targetSeat,
                                          "local-input-pointer",
                                          true);
        return true;
    }

    if (m_pendingXWaylandFocusOffers.contains(xwaylandSurface)) {
        qCDebug(lcTlXwayland) << "[XWL_FOCUS_ACCEPT_SKIP] XWayland focus offer already pending:"
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "input_model=" << xwaylandInputModelName(inputModel)
                               << "supports_wm_take_focus=" << supportsTakeFocus;
        return true;
    }

    if (!xwaylandSurface->offerFocus()) {
        qCDebug(lcTlXwayland) << "[XWL_FOCUS_ACCEPT_SKIP] XWayland focus offer was not sent:"
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "input_model=" << xwaylandInputModelName(inputModel)
                               << "supports_wm_take_focus=" << supportsTakeFocus;
        return true;
    }

    XWaylandFocusOffer offer;
    offer.wrapper = wrapper;
    offer.seat = targetSeat;
    offer.reason = reason;
    m_pendingXWaylandFocusOffers.insert(xwaylandSurface, offer);
    return true;
}

void Helper::commitXWaylandPopupTransientFocus(WXWaylandSurface *surface,
                                               SurfaceWrapper *wrapper,
                                               Qt::FocusReason reason,
                                               WSeat *seat,
                                               const char *source,
                                               bool requestNativeFocus)
{
    if (!surface || !wrapper || !seat)
        return;

    if (requestNativeFocus)
        m_pendingXWaylandFocusOffers.remove(surface);

    if (requestNativeFocus)
        surface->requestNativeFocus();

    requestKeyboardFocus(wrapper, reason, seat);

    XWaylandPopupFocusCommit commit;
    commit.wrapper = wrapper;
    commit.parentWrapper = xwaylandPopupTransientParentWrapper(wrapper, seat);
    commit.seat = seat;
    m_committedXWaylandPopupFocuses.insert(surface, commit);

    auto *parentWrapper = qobject_cast<SurfaceWrapper *>(commit.parentWrapper.data());
    qCDebug(lcTlXwayland) << "[XWL_POPUP_FOCUS_COMMIT] Committed explicit XWayland popup focus:"
                           << "source=" << source
                           << "window_id=" << xwaylandWindowId(wrapper)
                           << "surface=" << surface
                           << "wrapper=" << wrapper
                           << "parentWrapper=" << parentWrapper
                           << "seat=" << seat
                           << "reason=" << reason
                           << "request_native_focus=" << requestNativeFocus
                           << "input_model=" << xwaylandInputModelName(surface->inputModel())
                           << "geometry=" << wrapper->geometry();
}

bool Helper::commitXWaylandPointerGrabPopupFocus(SurfaceWrapper *wrapper,
                                                 Qt::FocusReason reason,
                                                 WSeat *seat,
                                                 const char *source)
{
    if (!wrapper || !seat || !xwaylandPopupPointerGrabOverridesParentFocus(wrapper))
        return false;

    auto *xwaylandSurface = xwaylandSurfaceForWrapper(wrapper);
    const bool mapped = wrapper->surface() && wrapper->surface()->mapped();
    const bool hasCapabilities = wrapper->hasActiveCapability() && wrapper->hasFocusCapability();
    const bool alreadyFocused = wrapper->surface()
        && seat->keyboardFocusSurface() == wrapper->surface();
    auto *current = workspace() ? workspace()->current() : nullptr;
    const bool currentWorkspace = current && wrapper->showOnWorkspace(current->id());

    auto logDecision = [&](const char *action, const char *why) {
        qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_GRAB_FOCUS] XWayland popup pointer grab focus:"
                               << "action=" << action
                               << "why=" << why
                               << "source=" << source
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "surface=" << xwaylandSurface
                               << "wrapper=" << wrapper
                               << "parentWrapper="
                               << xwaylandPopupTransientParentWrapper(wrapper, seat)
                               << "seat=" << seat
                               << "reason=" << reason
                               << "mapped=" << mapped
                               << "has_capabilities=" << hasCapabilities
                               << "already_focused=" << alreadyFocused
                               << "current_workspace=" << currentWorkspace
                               << "input_model="
                               << (xwaylandSurface
                                       ? xwaylandInputModelName(xwaylandSurface->inputModel())
                                       : xwaylandInputModelName(WXWaylandSurface::InputModelNone))
                               << "window_types="
                               << (xwaylandSurface ? xwaylandSurface->windowTypes()
                                                   : WXWaylandSurface::WindowTypes())
                               << "geometry=" << wrapper->geometry();
    };

    const char *skipReason = nullptr;
    if (!xwaylandSurface)
        skipReason = "missing-xwayland-surface";
    else if (!mapped)
        skipReason = "unmapped";
    else if (!hasCapabilities)
        skipReason = "missing-capability";
    else if (!currentWorkspace)
        skipReason = "not-current-workspace";
    else if (alreadyFocused)
        skipReason = "already-focused";

    if (skipReason) {
        logDecision("skip", skipReason);
        return false;
    }

    logDecision("commit", "x11-pointer-grab-focus");
    commitXWaylandPopupTransientFocus(xwaylandSurface,
                                      wrapper,
                                      reason,
                                      seat,
                                      source,
                                      false);
    return true;
}

void Helper::handleXWaylandRequestActivate(WXWaylandSurface *surface)
{
    SurfaceWrapper *sourceWrapper = nullptr;
    if (surface && m_rootSurfaceContainer)
        sourceWrapper = m_rootSurfaceContainer->getSurface(surface);

    WSeat *targetSeat = sourceWrapper ? getLastInteractingSeat(sourceWrapper) : nullptr;
    if (!targetSeat)
        targetSeat = m_primarySeat;

    SurfaceWrapper *targetWrapper = sourceWrapper;
    int redirectedDepth = 0;
    while (targetWrapper
           && xwaylandPopupKeepsParentFocusOnPointer(targetWrapper, targetSeat)
           && redirectedDepth < 32) {
        auto *parentWrapper = xwaylandPopupTransientParentWrapper(targetWrapper, targetSeat);
        if (!parentWrapper || parentWrapper == targetWrapper)
            break;
        targetWrapper = parentWrapper;
        ++redirectedDepth;
    }

    auto logDecision = [&](const char *action, const char *why, bool refreshNativeFocus) {
        qCDebug(lcTlXwayland) << "[XWL_REQUEST_ACTIVATE] Handle XWayland activation request:"
                               << "action=" << action
                               << "why=" << why
                               << "source_window_id=" << xwaylandWindowId(surface)
                               << "source_surface=" << surface
                               << "source_wrapper=" << sourceWrapper
                               << "source_popup_like="
                               << (sourceWrapper
                                       ? sourceWrapper->isXWaylandPopupLikeTransient()
                                       : false)
                               << "target_window_id=" << xwaylandWindowId(targetWrapper)
                               << "target_wrapper=" << targetWrapper
                               << "redirected_depth=" << redirectedDepth
                               << "seat=" << targetSeat
                               << "refresh_native_focus=" << refreshNativeFocus;
    };

    if (!sourceWrapper) {
        logDecision("skip", "missing-source-wrapper", false);
        return;
    }
    if (!targetWrapper) {
        logDecision("skip", "missing-target-wrapper", false);
        return;
    }
    if (!targetWrapper->surface() || !targetWrapper->surface()->mapped()) {
        logDecision("skip", "target-unmapped", false);
        return;
    }
    if (!targetWrapper->hasActiveCapability() || !targetWrapper->hasFocusCapability()) {
        logDecision("skip", "target-missing-capability", false);
        return;
    }
    if (!targetWrapper->showOnWorkspace(workspace()->current()->id())) {
        logDecision("skip", "target-not-on-current-workspace", false);
        return;
    }
    if (m_blockActivateSurface && targetWrapper->type() != SurfaceWrapper::Type::LockScreen) {
        logDecision("skip", "activation-blocked", false);
        return;
    }

    auto *seatContainer = m_rootSurfaceContainer->getSeatContainerOrDefault(targetSeat);
    if (!seatContainer) {
        logDecision("skip", "missing-seat-container", false);
        return;
    }

    const bool wasActivated = seatContainer->activatedSurface() == targetWrapper;
    const bool hadKeyboardFocus = seatContainer->keyboardFocusSurface() == targetWrapper;
    if (wasActivated && hadKeyboardFocus) {
        const bool sourceMapped = sourceWrapper->surface()
            && sourceWrapper->surface()->mapped();
        if (redirectedDepth > 0 && sourceMapped) {
            deferXWaylandPopupParentFocusRefresh(surface,
                                                 sourceWrapper,
                                                 targetWrapper,
                                                 targetSeat,
                                                 "popup-activation-request");
            logDecision("defer-native-focus-refresh",
                        "popup-parent-already-logically-focused",
                        false);
        } else {
            logDecision("keep-current-focus", "already-logically-focused", false);
        }
        return;
    }

    activateSurface(targetWrapper, Qt::ActiveWindowFocusReason, targetSeat);

    const bool isActivated = seatContainer->activatedSurface() == targetWrapper;
    const bool hasKeyboardFocus = seatContainer->keyboardFocusSurface() == targetWrapper;
    logDecision("activate",
                isActivated && hasKeyboardFocus ? "accepted" : "logical-focus-not-committed",
                false);
}

void Helper::acceptXWaylandFocus(WXWaylandSurface *surface, bool grab)
{
    auto pendingIt = m_pendingXWaylandFocusOffers.find(surface);
    const bool hasPendingOffer = pendingIt != m_pendingXWaylandFocusOffers.end();
    XWaylandFocusOffer pending;
    if (hasPendingOffer) {
        pending = pendingIt.value();
        m_pendingXWaylandFocusOffers.erase(pendingIt);
    }

    SurfaceWrapper *wrapper = nullptr;
    if (m_rootSurfaceContainer)
        wrapper = m_rootSurfaceContainer->getSurface(surface);
    if (!wrapper)
        wrapper = qobject_cast<SurfaceWrapper *>(pending.wrapper.data());

    WSeat *targetSeat = pending.seat.data();
    if (!targetSeat && wrapper)
        targetSeat = getLastInteractingSeat(wrapper);
    if (!targetSeat)
        targetSeat = m_primarySeat;

    const Qt::FocusReason reason =
        hasPendingOffer ? pending.reason : Qt::ActiveWindowFocusReason;
    auto logSkip = [&](const char *why) {
        qCDebug(lcTlXwayland) << "[XWL_FOCUS_ACCEPT_SKIP] Skip accepted XWayland focus:"
                               << "why=" << why
                               << "window_id=" << xwaylandWindowId(surface)
                               << "surface=" << surface
                               << "wrapper=" << wrapper
                               << "seat=" << targetSeat
                               << "grab=" << grab
                               << "has_pending_offer=" << hasPendingOffer;
    };

    if (!wrapper) {
        logSkip("missing-wrapper");
        return;
    }
    if (!hasPendingOffer && !grab) {
        if (auto *popupOwner = activeXWaylandPopupPointerOwner(targetSeat)) {
            if (wrapper == popupOwner) {
                qCDebug(lcTlXwayland) << "[XWL_POPUP_FOCUS_OBSERVED] Ignore unsolicited popup focus-in during pointer ownership:"
                                       << "window_id=" << xwaylandWindowId(wrapper)
                                       << "wrapper=" << wrapper
                                       << "seat=" << targetSeat
                                       << "surface=" << surface;
                return;
            }

            SurfaceWrapper *popupParent = popupOwner->parentSurface();
            if (!popupParent) {
                const auto ownerIt = m_xwaylandPopupPointerOwners.constFind(targetSeat);
                if (ownerIt != m_xwaylandPopupPointerOwners.constEnd())
                    popupParent = qobject_cast<SurfaceWrapper *>(ownerIt->parentWrapper.data());
            }
            if (wrapper == popupParent) {
                qCDebug(lcTlXwayland) << "[XWL_POPUP_PARENT_FOCUS_KEEP] Keep parent focus while popup pointer owner is active:"
                                       << "parent_window_id=" << xwaylandWindowId(wrapper)
                                       << "parentWrapper=" << wrapper
                                       << "popup_window_id=" << xwaylandWindowId(popupOwner)
                                       << "popupWrapper=" << popupOwner
                                       << "seat=" << targetSeat;
                return;
            }
        }

        logSkip("focus-in-without-offer");
        return;
    }
    if (wrapper->isIMCandidatePanel()) {
        logSkip("im-candidate-panel");
        return;
    }
    if (!wrapper->surface() || !wrapper->surface()->mapped()) {
        logSkip("unmapped");
        return;
    }
    if (!wrapper->hasActiveCapability() || !wrapper->hasFocusCapability()) {
        logSkip("missing-capability");
        return;
    }
    if (!wrapper->showOnWorkspace(workspace()->current()->id())) {
        logSkip("not-current-workspace");
        return;
    }

    const bool popupTransient = wrapper->isXWaylandPopupLikeTransient();
    const bool activateGlobal = !popupTransient;
    qCDebug(lcTlXwayland) << "[XWL_FOCUS_ACCEPT_COMMIT] Commit accepted XWayland focus:"
                           << "window_id=" << xwaylandWindowId(wrapper)
                           << "surface=" << surface
                           << "wrapper=" << wrapper
                           << "seat=" << targetSeat
                           << "reason=" << reason
                           << "grab=" << grab
                           << "has_pending_offer=" << hasPendingOffer
                           << "popup_transient=" << popupTransient
                           << "activate_global=" << activateGlobal
                           << "geometry=" << wrapper->geometry();

    if (popupTransient) {
        commitXWaylandPopupTransientFocus(surface,
                                          wrapper,
                                          reason,
                                          targetSeat,
                                          grab ? "x11-grab-focus" : "accepted-offer",
                                          false);
        return;
    }

    if (!m_blockActivateSurface || wrapper->type() == SurfaceWrapper::Type::LockScreen) {
        setActivatedSurface(wrapper, targetSeat);
        requestKeyboardFocus(wrapper, reason, targetSeat);
    } else {
        workspace()->pushActivedSurface(wrapper);
    }
}

void Helper::acceptXWaylandPointerGrabFocus(WXWaylandSurface *surface)
{
    if (!surface)
        return;

    const bool alreadyKnown = m_xwaylandPointerGrabFocusSurfaces.contains(surface);
    if (!alreadyKnown) {
        m_xwaylandPointerGrabFocusSurfaces.insert(surface);
        connect(surface, &QObject::destroyed, this, [this, surface] {
            m_xwaylandPointerGrabFocusSurfaces.remove(surface);
        });
    }

    SurfaceWrapper *wrapper = nullptr;
    if (m_rootSurfaceContainer)
        wrapper = m_rootSurfaceContainer->getSurface(surface);

    WSeat *targetSeat = wrapper ? getLastInteractingSeat(wrapper) : nullptr;
    if (!targetSeat)
        targetSeat = m_primarySeat;

    qCDebug(lcTlXwayland) << "[XWL_POINTER_GRAB_FOCUS] Accepted XWayland pointer grab focus:"
                           << "window_id=" << xwaylandWindowId(surface)
                           << "surface=" << surface
                           << "wrapper=" << wrapper
                           << "parentWrapper="
                           << (wrapper ? xwaylandPopupTransientParentWrapper(wrapper, targetSeat)
                                       : nullptr)
                           << "seat=" << targetSeat
                           << "already_known=" << alreadyKnown
                           << "popup_like="
                           << (wrapper ? wrapper->isXWaylandPopupLikeTransient() : false)
                           << "overrides_parent_focus="
                           << (wrapper ? xwaylandPopupPointerGrabOverridesParentFocus(wrapper)
                                       : false)
                           << "input_model=" << xwaylandInputModelName(surface->inputModel())
                           << "window_types=" << surface->windowTypes()
                           << "geometry=" << (wrapper ? wrapper->geometry() : QRectF());

    commitXWaylandPointerGrabPopupFocus(wrapper,
                                        Qt::MouseFocusReason,
                                        targetSeat,
                                        "x11-pointer-grab-focus");
}

void Helper::clearXWaylandFocusOffer(WXWaylandSurface *surface)
{
    if (m_pendingXWaylandFocusOffers.remove(surface) > 0) {
        qCDebug(lcTlXwayland) << "[XWL_FOCUS_ACCEPT_SKIP] Cleared pending XWayland focus offer:"
                               << "window_id=" << xwaylandWindowId(surface)
                               << "surface=" << surface;
    }
}

void Helper::clearXWaylandPointerGrabFocus(WXWaylandSurface *surface, const char *reason)
{
    if (m_xwaylandPointerGrabFocusSurfaces.remove(surface) > 0) {
        qCDebug(lcTlXwayland) << "[XWL_POINTER_GRAB_FOCUS] Cleared XWayland pointer grab focus:"
                               << "reason=" << reason
                               << "window_id=" << xwaylandWindowId(surface)
                               << "surface=" << surface;
    }
}

void Helper::clearXWaylandPopupFocusState(WXWaylandSurface *surface)
{
    const bool hadCommittedFocus = m_committedXWaylandPopupFocuses.contains(surface);
    clearDeferredXWaylandPopupTransientFocus(surface, "clear-state");
    clearXWaylandPopupPointerOwner(surface, "clear-state");
    clearXWaylandPointerButtonSequence(surface, "clear-state");
    clearXWaylandPopupOpeningPointerGuard(surface, "clear-state");
    clearXWaylandPointerGrabFocus(surface, "clear-state");
    clearXWaylandFocusOffer(surface);
    restoreXWaylandPopupTransientFocus(surface, "clear-state");

    WSeat *refreshSeat = nullptr;
    for (auto it = m_xwaylandPopupParentFocusRefreshes.constBegin();
         it != m_xwaylandPopupParentFocusRefreshes.constEnd(); ++it) {
        if (it->surface.data() == surface) {
            refreshSeat = it.key();
            break;
        }
    }
    if (refreshSeat) {
        if (hadCommittedFocus) {
            m_xwaylandPopupParentFocusRefreshes.remove(refreshSeat);
            qCDebug(lcTlXwayland) << "[XWL_POPUP_PARENT_FOCUS_REFRESH] Cancel redundant XWayland popup parent focus refresh:"
                                   << "reason= committed-popup-focus-restored"
                                   << "window_id=" << xwaylandWindowId(surface)
                                   << "surface=" << surface
                                   << "seat=" << refreshSeat;
        } else {
            scheduleXWaylandPopupParentFocusRefresh(refreshSeat, "popup-focus-state-cleared");
        }
    }

    // A keep-parent-focus popup never received compositor-committed focus, so
    // there is nothing to restore when it disappears. In particular, do not
    // manufacture a popup -> parent focus transition here: this function runs
    // once for unmap and again for dissociation. Committed popup focus is
    // restored exactly once by restoreXWaylandPopupTransientFocus().
    qCDebug(lcTlXwayland) << "[XWL_POPUP_FOCUS_CLEAR] Cleared XWayland popup focus state:"
                           << "window_id=" << xwaylandWindowId(surface)
                           << "surface=" << surface
                           << "had_committed_focus=" << hadCommittedFocus;
}

void Helper::deferXWaylandPopupParentFocusRefresh(WXWaylandSurface *surface,
                                                  SurfaceWrapper *wrapper,
                                                  SurfaceWrapper *parentWrapper,
                                                  WSeat *seat,
                                                  const char *reason)
{
    if (!surface || !wrapper || !parentWrapper || !seat)
        return;

    const auto oldRefresh = m_xwaylandPopupParentFocusRefreshes.value(seat);
    auto &refresh = m_xwaylandPopupParentFocusRefreshes[seat];
    refresh.surface = surface;
    refresh.wrapper = wrapper;
    refresh.parentWrapper = parentWrapper;
    refresh.seat = seat;
    refresh.windowId = xwaylandWindowId(surface);
    refresh.scheduled = oldRefresh.scheduled;

    if (!m_xwaylandPopupParentFocusRefreshSeats.contains(seat)) {
        m_xwaylandPopupParentFocusRefreshSeats.insert(seat);
        connect(seat, &QObject::destroyed, this, [this, seat] {
            m_xwaylandPopupParentFocusRefreshes.remove(seat);
            m_xwaylandPopupParentFocusRefreshSeats.remove(seat);
        });
    }

    qCDebug(lcTlXwayland) << "[XWL_POPUP_PARENT_FOCUS_REFRESH] Defer XWayland popup parent native focus refresh:"
                           << "reason=" << reason
                           << "window_id=" << refresh.windowId
                           << "surface=" << surface
                           << "wrapper=" << wrapper
                           << "parent_window_id=" << xwaylandWindowId(parentWrapper)
                           << "parentWrapper=" << parentWrapper
                           << "seat=" << seat
                           << "replaced_window_id=" << oldRefresh.windowId
                           << "already_scheduled=" << oldRefresh.scheduled;
}

void Helper::scheduleXWaylandPopupParentFocusRefresh(WSeat *seat, const char *reason)
{
    auto refreshIt = m_xwaylandPopupParentFocusRefreshes.find(seat);
    if (refreshIt == m_xwaylandPopupParentFocusRefreshes.end())
        return;

    auto *sourceWrapper = qobject_cast<SurfaceWrapper *>(refreshIt->wrapper.data());
    const bool sourceMapped = sourceWrapper && sourceWrapper->surface()
        && sourceWrapper->surface()->mapped();
    const auto sequenceIt = m_xwaylandPointerButtonSequences.constFind(seat);
    const int pressedButtons = sequenceIt != m_xwaylandPointerButtonSequences.constEnd()
        ? sequenceIt->pressedButtons
        : 0;

    if (sourceMapped || pressedButtons > 0) {
        qCDebug(lcTlXwayland) << "[XWL_POPUP_PARENT_FOCUS_REFRESH] Keep XWayland popup parent focus refresh pending:"
                               << "reason=" << reason
                               << "window_id=" << refreshIt->windowId
                               << "wrapper=" << sourceWrapper
                               << "parentWrapper=" << refreshIt->parentWrapper.data()
                               << "seat=" << seat
                               << "source_mapped=" << sourceMapped
                               << "pressed_buttons=" << pressedButtons
                               << "already_scheduled=" << refreshIt->scheduled;
        return;
    }

    if (refreshIt->scheduled)
        return;

    refreshIt->scheduled = true;
    const QByteArray queuedReason(reason);
    QPointer<WSeat> guardedSeat(seat);
    QMetaObject::invokeMethod(this,
                              [this, guardedSeat, queuedReason] {
                                  if (auto *rawSeat = guardedSeat.data()) {
                                      restoreXWaylandPopupParentFocus(rawSeat,
                                                                     queuedReason.constData());
                                  }
                              },
                              Qt::QueuedConnection);

    qCDebug(lcTlXwayland) << "[XWL_POPUP_PARENT_FOCUS_REFRESH] Schedule XWayland popup parent native focus refresh:"
                           << "reason=" << reason
                           << "window_id=" << refreshIt->windowId
                           << "wrapper=" << sourceWrapper
                           << "parentWrapper=" << refreshIt->parentWrapper.data()
                           << "seat=" << seat;
}

void Helper::restoreXWaylandPopupParentFocus(WSeat *seat, const char *reason)
{
    auto refreshIt = m_xwaylandPopupParentFocusRefreshes.find(seat);
    if (refreshIt == m_xwaylandPopupParentFocusRefreshes.end())
        return;

    refreshIt->scheduled = false;
    auto *sourceWrapper = qobject_cast<SurfaceWrapper *>(refreshIt->wrapper.data());
    auto *parentWrapper = qobject_cast<SurfaceWrapper *>(refreshIt->parentWrapper.data());
    const bool sourceMapped = sourceWrapper && sourceWrapper->surface()
        && sourceWrapper->surface()->mapped();
    const auto sequenceIt = m_xwaylandPointerButtonSequences.constFind(seat);
    const int pressedButtons = sequenceIt != m_xwaylandPointerButtonSequences.constEnd()
        ? sequenceIt->pressedButtons
        : 0;

    auto logCancel = [&](const char *why) {
        qCDebug(lcTlXwayland) << "[XWL_POPUP_PARENT_FOCUS_REFRESH] Cancel XWayland popup parent native focus refresh:"
                               << "why=" << why
                               << "reason=" << reason
                               << "window_id=" << refreshIt->windowId
                               << "wrapper=" << sourceWrapper
                               << "parentWrapper=" << parentWrapper
                               << "seat=" << seat
                               << "source_mapped=" << sourceMapped
                               << "pressed_buttons=" << pressedButtons;
    };

    if (sourceMapped) {
        qCDebug(lcTlXwayland) << "[XWL_POPUP_PARENT_FOCUS_REFRESH] Postpone XWayland popup parent native focus refresh:"
                               << "reason=" << reason
                               << "window_id=" << refreshIt->windowId
                               << "wrapper=" << sourceWrapper
                               << "parentWrapper=" << parentWrapper
                               << "seat=" << seat
                               << "source_mapped=" << sourceMapped;
        return;
    }
    if (pressedButtons > 0) {
        qCDebug(lcTlXwayland) << "[XWL_POPUP_PARENT_FOCUS_REFRESH] Postpone XWayland popup parent native focus refresh:"
                               << "reason=" << reason
                               << "window_id=" << refreshIt->windowId
                               << "wrapper=" << sourceWrapper
                               << "parentWrapper=" << parentWrapper
                               << "seat=" << seat
                               << "pressed_buttons=" << pressedButtons;
        return;
    }
    if (!parentWrapper) {
        logCancel("missing-parent-wrapper");
        m_xwaylandPopupParentFocusRefreshes.erase(refreshIt);
        return;
    }

    auto *seatContainer = m_rootSurfaceContainer
        ? m_rootSurfaceContainer->getSeatContainerOrDefault(seat)
        : nullptr;
    if (!seatContainer || seatContainer->activatedSurface() != parentWrapper
        || seatContainer->keyboardFocusSurface() != parentWrapper) {
        logCancel("parent-not-logically-focused");
        m_xwaylandPopupParentFocusRefreshes.erase(refreshIt);
        return;
    }
    if (!parentWrapper->surface() || !parentWrapper->surface()->mapped()) {
        logCancel("parent-unmapped");
        m_xwaylandPopupParentFocusRefreshes.erase(refreshIt);
        return;
    }
    if (!parentWrapper->hasActiveCapability() || !parentWrapper->hasFocusCapability()) {
        logCancel("parent-missing-capability");
        m_xwaylandPopupParentFocusRefreshes.erase(refreshIt);
        return;
    }
    if (!workspace() || !workspace()->current()
        || !parentWrapper->showOnWorkspace(workspace()->current()->id())) {
        logCancel("parent-not-on-current-workspace");
        m_xwaylandPopupParentFocusRefreshes.erase(refreshIt);
        return;
    }

    auto *parentXWaylandSurface = xwaylandSurfaceForWrapper(parentWrapper);
    if (!parentXWaylandSurface) {
        logCancel("missing-parent-xwayland-surface");
        m_xwaylandPopupParentFocusRefreshes.erase(refreshIt);
        return;
    }

    const uint32_t sourceWindowId = refreshIt->windowId;
    const uint32_t parentWindowId = xwaylandWindowId(parentWrapper);
    m_xwaylandPopupParentFocusRefreshes.erase(refreshIt);

    // Erase the one-shot transaction before forcing X focus. FocusIn may make
    // the client send another _NET_ACTIVE_WINDOW request; without a pending
    // popup transaction that feedback is intentionally a no-op.
    parentXWaylandSurface->forceNativeFocus();
    requestKeyboardFocus(parentWrapper, Qt::OtherFocusReason, seat);

    qCDebug(lcTlXwayland) << "[XWL_POPUP_PARENT_FOCUS_REFRESH] Refreshed XWayland popup parent native focus:"
                           << "reason=" << reason
                           << "window_id=" << sourceWindowId
                           << "parent_window_id=" << parentWindowId
                           << "parentWrapper=" << parentWrapper
                           << "seat=" << seat;
}

void Helper::restoreXWaylandPopupTransientFocus(WXWaylandSurface *surface, const char *reason)
{
    auto committedIt = m_committedXWaylandPopupFocuses.find(surface);
    if (committedIt == m_committedXWaylandPopupFocuses.end())
        return;

    XWaylandPopupFocusCommit commit = committedIt.value();
    m_committedXWaylandPopupFocuses.erase(committedIt);

    auto *wrapper = qobject_cast<SurfaceWrapper *>(commit.wrapper.data());
    auto *parentWrapper = qobject_cast<SurfaceWrapper *>(commit.parentWrapper.data());
    if (!parentWrapper && wrapper)
        parentWrapper = xwaylandPopupTransientParentWrapper(wrapper, commit.seat.data());

    WSeat *targetSeat = commit.seat.data();
    if (!targetSeat && parentWrapper)
        targetSeat = getLastInteractingSeat(parentWrapper);
    if (!targetSeat)
        targetSeat = m_primarySeat;

    SurfaceWrapper *activeSurface = nullptr;
    if (m_rootSurfaceContainer) {
        if (auto *seatContainer = m_rootSurfaceContainer->getSeatContainerOrDefault(targetSeat))
            activeSurface = seatContainer->activatedSurface();
    }

    auto logSkip = [&](const char *why) {
        qCDebug(lcTlXwayland) << "[XWL_POPUP_FOCUS_RESTORE_SKIP] Skip restoring XWayland popup parent focus:"
                               << "why=" << why
                               << "reason=" << reason
                               << "window_id=" << xwaylandWindowId(surface)
                               << "surface=" << surface
                               << "wrapper=" << wrapper
                               << "parentWrapper=" << parentWrapper
                               << "activeSurface=" << activeSurface
                               << "seat=" << targetSeat;
    };

    if (!parentWrapper) {
        logSkip("missing-parent-wrapper");
        return;
    }
    if (activeSurface != parentWrapper) {
        logSkip("parent-not-active");
        return;
    }
    if (!parentWrapper->surface() || !parentWrapper->surface()->mapped()) {
        logSkip("parent-unmapped");
        return;
    }
    if (!parentWrapper->hasFocusCapability()) {
        logSkip("parent-missing-focus-capability");
        return;
    }

    auto *parentXWaylandSurface =
        qobject_cast<WXWaylandSurface *>(parentWrapper->shellSurface());
    if (!parentXWaylandSurface) {
        logSkip("missing-parent-xwayland-surface");
        return;
    }

    parentXWaylandSurface->forceNativeFocus();
    requestKeyboardFocus(parentWrapper, Qt::OtherFocusReason, targetSeat);

    qCDebug(lcTlXwayland) << "[XWL_POPUP_FOCUS_RESTORE] Restored XWayland popup parent focus:"
                           << "reason=" << reason
                           << "window_id=" << xwaylandWindowId(surface)
                           << "surface=" << surface
                           << "wrapper=" << wrapper
                           << "parent_window_id=" << xwaylandWindowId(parentWrapper)
                           << "parentWrapper=" << parentWrapper
                           << "seat=" << targetSeat
                           << "parent_geometry=" << parentWrapper->geometry();
}

void Helper::forceActivateSurface(SurfaceWrapper *wrapper, Qt::FocusReason reason, WSeat *seat)
{
    if (!wrapper) {
        qCCritical(lcTlShell) << "Don't force activate to empty surface! do you want `Helper::activeSurface(nullptr)`?";
        return;
    }
    if (!wrapper->shellSurface()) {
        qCWarning(lcTlShell) << "Try to force activate a destroyed surface!";
        return;
    }

    // Forced activation: the modal's minimized state is irrelevant — the caller explicitly
    // requested this surface. However, if the parent itself is minimized, it must be restored
    // here because the modal (which may already be unminimized) won't trigger parent linkage.
    SurfaceWrapper *originalWrapper = wrapper;
    const bool restoreAnimation =
        !(reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason);
    if (SurfaceWrapper *modal = wrapper->findModal()) {
        if (modal != wrapper) {
            if (modal->workspaceId() != wrapper->workspaceId() && wrapper->workspaceId() != -1) {
                workspace()->moveSurfaceTo(modal, wrapper->workspaceId());
            }
            if (originalWrapper->isMinimized()) {
                originalWrapper->restoreFromMinimized(restoreAnimation);
            }
            originalWrapper->stackToLast();
            wrapper = modal;
        }
    }

    restoreFromShowDesktop(wrapper);

    if (wrapper->isMinimized()) {
        wrapper->restoreFromMinimized(restoreAnimation);
    }

    if (!wrapper->surface()->mapped()) {
        qCWarning(lcTlShell) << "Can't activate unmapped surface: " << wrapper;
        return;
    }

    if (!wrapper->showOnWorkspace(workspace()->current()->id()))
        workspace()->switchTo(workspace()->modelIndexOfSurface(wrapper));
    Helper::instance()->activateSurface(wrapper, reason, seat);
}

RootSurfaceContainer *Helper::rootSurfaceContainer() const
{
    return m_rootSurfaceContainer;
}

void Helper::fakePressSurfaceBottomRightToReszie(SurfaceWrapper *surface)
{
    auto position = surface->geometry().bottomRight();
    m_fakelastPressedPosition = position;
    m_primarySeat->setCursorPosition(position);
    Q_EMIT surface->resizeRequested(Qt::BottomEdge | Qt::RightEdge);
}

bool Helper::beforeDisposeEvent(WSeat *seat, QWindow *targetWindow, QInputEvent *event)
{
    if (!m_instance || !m_renderWindow || !m_backend) {
        return false;
    }

    if (Q_UNLIKELY(!targetWindow || !event)) {
        return false;
    }

    if (event->isInputEvent()) {
        m_idleNotifier->notify_activity(seat->nativeHandle());

        // Wake DPMS-off outputs on any input event
        // Only re-enable outputs disabled by output_power, not user-disabled outputs
        for (auto *out : std::as_const(m_outputList)) {
            auto *wlr_out = out->output()->nativeHandle();
            if (!wlr_out->enabled && wlr_out->current_mode && m_powerOffOutputs.contains(wlr_out)) {
                qw_output_state state;
                state.set_enabled(true);
                if (!out->output()->handle()->commit_state(state)) {
                    qCWarning(lcTlCore) << "Failed to wake output" << wlr_out->name;
                } else {
                    m_powerOffOutputs.remove(wlr_out);
                }
            }
        }
    }

    if (event->type() == QEvent::KeyPress) {
        auto kevent = static_cast<QKeyEvent *>(event);
        const auto modifiers = kevent->modifiers();
        const auto ctrlAlt = Qt::ControlModifier | Qt::AltModifier;
        if ((modifiers & ctrlAlt) == ctrlAlt) {
            const auto key = kevent->key();
            if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
                const int vtnr = key - Qt::Key_F1 + 1;
                const bool sessionActive = m_backend->isSessionActive();
                qCWarning(lcTlCore) << "Ctrl+Alt+Fn VT shortcut received"
                                        << vtnr << "sessionActive" << sessionActive;
                if (!sessionActive) {
                    return true;
                }

                qCWarning(lcTlCore) << "Ctrl+Alt+Fn VT shortcut requested" << vtnr;
                m_backend->session()->change_vt(vtnr);
                return true;
            }
        }
    }

    WSeat *targetSeat = seat;
    if (event->device()) {
        WInputDevice *device = WInputDevice::from(event->device());
        if (device) {
            targetSeat = m_seatManager->getSeatForDevice(device);
            if (!targetSeat) {
                qCWarning(lcTlCore) << "Device has no associated seat, using default seat";
                targetSeat = seat;
            }
        }
    }

    if (targetSeat && targetSeat != seat) {
        return false;
    }
    m_currentEventSeat = targetSeat;
    [[maybe_unused]] auto clearEventSeat = qScopeGuard([this] { m_currentEventSeat = nullptr; });

    // A popup can unmap itself while handling a button press. In that case its QQuick
    // event item may be gone before the physical release reaches surface dispatch, so
    // finish the client-side sequence at the window-level input stage.
    if (targetSeat && event->type() == QEvent::MouseButtonRelease
        && finishXWaylandPendingPointerButtonRelease(targetSeat,
                                                      event,
                                                      "before-dispose-pending-release")) {
        return true;
    }

    if (seat == m_primarySeat) {
        if (event->type() == QEvent::KeyPress) {
            auto kevent = static_cast<QKeyEvent *>(event);
            switch (kevent->key()) {
            case Qt::Key_Meta:
            case Qt::Key_Super_L:
            case Qt::Key_Super_R:
                if (auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(seat))
                    seatContainer->setMetaKeyPressed(true);
                break;
            default:
                if (auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(seat))
                    seatContainer->setMetaKeyPressed(false);
                break;
            }
        }

        if (event->type() == QEvent::KeyPress) {
            auto kevent = static_cast<QKeyEvent *>(event);

#ifndef QT_NO_DEBUG
            if (QKeySequence(kevent->keyCombination()) ==
                QKeySequence(Qt::MetaModifier | Qt::Key_F12)) {
                std::terminate();
            }
            // The debug view shortcut should always handled first
            if (QKeySequence(kevent->keyCombination())
                == QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier | Qt::Key_F11)) {
                if (toggleDebugMenuBar())
                    return true;
            }
#endif

            if (m_captureSelector) {
                if (event->modifiers() == Qt::NoModifier && kevent->key() == Qt::Key_Escape)
                    m_captureSelector->cancelSelection();
            }
        }

        if (event->type() == QEvent::KeyRelease && !m_captureSelector) {
            auto kevent = static_cast<QKeyEvent *>(event);
            const int key = kevent->key();
            if (key == Qt::Key_Alt || key == Qt::Key_Control || key == Qt::Key_Shift
                || key == Qt::Key_Meta) {
                Q_EMIT modifierKeyReleased(kevent);
            }
        }
    }

    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) {
        handleLeftButtonStateChanged(event);
    }

    if (event->type() == QEvent::Wheel) {
        handleWhellValueChanged(event);
    }

    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress) {
        seat->cursor()->setVisible(true);
    } else if (event->type() == QEvent::TouchBegin) {
        seat->cursor()->setVisible(false);
    }

    doGesture(event);

    // Per-seat move/resize handling
    const auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(seat);
    if (seatContainer && seatContainer->moveResizeState().surface) {
        if (Q_LIKELY(event->type() == QEvent::MouseMove || event->type() == QEvent::TouchUpdate)) {
            auto cursor = seat->cursor();
            Q_ASSERT(cursor);
            QMouseEvent *ev = static_cast<QMouseEvent *>(event);

            const auto &moveResizeState = seatContainer->moveResizeState();
            auto ownsOutput = moveResizeState.surface->ownsOutput();
            if (!ownsOutput) {
                m_rootSurfaceContainer->endMoveResizeForSeat(seat);
                return false;
            }

            auto increment_pos = ev->globalPosition() - moveResizeState.initialPosition;
            m_rootSurfaceContainer->doMoveResizeForSeat(seat, increment_pos);

            return true;
        } else if (event->type() == QEvent::KeyPress
                   && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            m_rootSurfaceContainer->cancelMoveResizeForSeat(seat);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease
                   || event->type() == QEvent::TouchEnd) {
            m_rootSurfaceContainer->endMoveResizeForSeat(seat);
        }
    }

    // Capture mode: intercept key events before dispatchKeyEvent
    if (m_shortcutManager->isCaptureActive() && m_shortcutManager->tryHandleCaptureEvent(seat, event))
        return true;

    if (seat == m_primarySeat && !m_captureSelector && m_currentMode != CurrentMode::LockScreen
        && (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)) {
        auto kevent = static_cast<QKeyEvent *>(event);
        auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(seat);

        // Meta: consume press as modifier; suppress release when used in combo
        if (kevent->key() == Qt::Key_Meta || kevent->key() == Qt::Key_Super_L || kevent->key() == Qt::Key_Super_R) {
            if (kevent->type() == QEvent::KeyPress) {
                return true;
            }
            if (kevent->type() == QEvent::KeyRelease && seatContainer && !seatContainer->metaKeyPressed()) {
                return false;
            }
        }

        if (m_shortcutManager->controller()->dispatchKeyEvent(kevent)) {
            return true;
        }
    }

    return false;
}

bool Helper::beforeHandleEvent(WSeat *seat,
                               WSurface *watched,
                               QObject *surfaceItem,
                               QObject *eventObject,
                               QInputEvent *event)
{
    if (!m_instance || !m_renderWindow || !m_backend || !event)
        return false;

    if (!isXWaylandPopupPointerEvent(event))
        return false;

    auto *wrapper = m_rootSurfaceContainer ? m_rootSurfaceContainer->getSurface(watched) : nullptr;
    if (filterXWaylandPendingPointerButtonSequence(seat,
                                                   wrapper,
                                                   event,
                                                   "before-handle-pending-sequence")) {
        return true;
    }

    if (redirectXWaylandPopupPointerEvent(seat, wrapper, surfaceItem, event))
        return true;

    if (!wrapper || !wrapper->isXWaylandPopupLikeTransient())
        return false;

    if (xwaylandPointerButtonSequenceBlocksPopup(wrapper, seat)) {
        auto *sequenceWrapper = activeXWaylandPointerButtonSequenceWrapper(seat);
        const auto sequenceIt = m_xwaylandPointerButtonSequences.constFind(seat);
        const int sequencePressedButtons =
            sequenceIt != m_xwaylandPointerButtonSequences.constEnd()
            ? sequenceIt->pressedButtons
            : 0;

        if (event->type() == QEvent::MouseButtonRelease && sequenceWrapper) {
            if (redirectXWaylandPopupBlockedRelease(wrapper,
                                                    sequenceWrapper,
                                                    seat,
                                                    event,
                                                    "direct-skip-sequence-release")) {
                return true;
            }
            if (recoverXWaylandPointerButtonSequenceRelease(seat,
                                                            event,
                                                            sequenceWrapper,
                                                            "direct-skip-sequence-release-fallback")) {
                return true;
            }
            return false;
        }

        if (xwaylandPointerButtonSequenceAllowsRelatedPopupMove(wrapper, seat, event)) {
            qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_DIRECT_RELATED_MOVE] Allow direct related XWayland popup pointer move during active sequence:"
                                   << "event_type=" << event->type()
                                   << "window_id=" << xwaylandWindowId(wrapper)
                                   << "wrapper=" << wrapper
                                   << "parentWrapper="
                                   << xwaylandPopupTransientParentWrapper(wrapper, seat)
                                   << "sequence_window_id="
                                   << xwaylandWindowId(sequenceWrapper)
                                   << "sequence_wrapper=" << sequenceWrapper
                                   << "sequence_pressed_buttons="
                                   << sequencePressedButtons
                                   << "seat=" << seat
                                   << "watched=" << watched
                                   << "surfaceItem=" << surfaceItem
                                   << "scene_pos=" << pointerEventScenePosition(event)
                                   << "wrapper_geometry=" << wrapper->geometry();
            if (redirectDirectXWaylandPopupPointerEvent(wrapper, seat, eventObject, event))
                return true;
        }

        if (!isXWaylandPopupPointerEnterOrMove(event))
            return false;

        qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_DIRECT_SKIP] Skip direct XWayland popup pointer event:"
                               << "reason= active-button-sequence-on-other-surface"
                               << "event_type=" << event->type()
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "parentWrapper=" << xwaylandPopupTransientParentWrapper(wrapper, seat)
                               << "sequence_window_id=" << xwaylandWindowId(sequenceWrapper)
                               << "sequence_wrapper=" << sequenceWrapper
                               << "sequence_pressed_buttons=" << sequencePressedButtons
                               << "seat=" << seat
                               << "watched=" << watched
                               << "surfaceItem=" << surfaceItem
                               << "scene_pos=" << pointerEventScenePosition(event)
                               << "wrapper_geometry=" << wrapper->geometry();
        return true;
    }

    if (redirectDirectXWaylandPopupPointerEvent(wrapper, seat, eventObject, event))
        return true;

    auto *popupEventObject = wrapper->surfaceItem()
        ? wrapper->surfaceItem()->eventItem()
        : nullptr;
    const bool eventObjectMatches = popupEventObject && eventObject == popupEventObject;
    const bool pointerFocusMatches = eventObjectMatches && wrapper->surface() && seat
        && seat->pointerFocusMatches(wrapper->surface(), eventObject);
    const bool pointerHasGrab = seat && seat->pointerHasGrab();
    if (pointerFocusMatches && !pointerHasGrab) {
        if (filterXWaylandPopupOpeningPointerEvent(wrapper,
                                                  seat,
                                                  event,
                                                  "direct-native-pointer-event")) {
            return true;
        }

        const auto *pointEvent =
            event->isSinglePointEvent() ? static_cast<QSinglePointEvent *>(event) : nullptr;
        qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_DIRECT_NATIVE] Use native delivery for direct XWayland popup pointer event:"
                               << "dispatch_mode= native"
                               << "event_type=" << event->type()
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "parentWrapper="
                               << xwaylandPopupTransientParentWrapper(wrapper, seat)
                               << "seat=" << seat
                               << "eventObject=" << eventObject
                               << "expected_event_object=" << popupEventObject
                               << "event_object_matches=" << eventObjectMatches
                               << "pointer_focus_surface=" << seat->pointerFocusSurface()
                               << "pointer_focus_matches=" << pointerFocusMatches
                               << "pointer_has_grab=" << pointerHasGrab
                               << "local_pos="
                               << (pointEvent ? pointEvent->position() : QPointF())
                               << "scene_pos=" << pointerEventScenePosition(event)
                               << "global_pos=" << pointerEventGlobalPosition(event)
                               << "wrapper_geometry=" << wrapper->geometry();
        setXWaylandPopupPointerOwner(wrapper, seat, "direct-native-pointer-event");

        if (event->isSinglePointEvent()
            && static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
            updateSurfaceSeatInteraction(wrapper, seat);
            offerXWaylandPopupTransientFocus(wrapper, Qt::MouseFocusReason, seat);
        }

        return false;
    }

    const auto *pointEvent =
        event->isSinglePointEvent() ? static_cast<QSinglePointEvent *>(event) : nullptr;
    qCDebug(lcTlXwayland) << "[XWL_POPUP_POINTER_PREPARE] Prepare XWayland popup pointer event:"
                           << "event_type=" << event->type()
                           << "window_id=" << xwaylandWindowId(wrapper)
                           << "wrapper=" << wrapper
                           << "parentWrapper=" << xwaylandPopupTransientParentWrapper(wrapper, seat)
                           << "seat=" << seat
                           << "watched=" << watched
                           << "surfaceItem=" << surfaceItem
                           << "local_pos="
                           << (pointEvent ? pointEvent->position() : QPointF())
                           << "global_pos="
                           << (pointEvent ? pointEvent->globalPosition() : QPointF())
                           << "wrapper_geometry=" << wrapper->geometry();

    setXWaylandPopupPointerOwner(wrapper, seat, "direct-popup-pointer-event");

    return false;
}

bool Helper::afterHandleEvent([[maybe_unused]] WSeat *seat,
                              WSurface *watched,
                              QObject *surfaceItem,
                              QObject *,
                              QInputEvent *event)
{
    if (!m_instance || !m_renderWindow || !m_backend)
        return false;

    if (event->isSinglePointEvent() && isXWaylandPopupPointerEvent(event)) {
        // surfaceItem is qml type: XdgSurfaceItem or LayerSurfaceItem
        auto *waylandSurfaceItem = qobject_cast<WSurfaceItem *>(surfaceItem);
        if (!waylandSurfaceItem)
            return false;

        auto toplevelSurface = waylandSurfaceItem->shellSurface();
        if (!toplevelSurface)
            return false;
        Q_ASSERT(toplevelSurface->surface() == watched);

        auto surface = m_rootSurfaceContainer->getSurface(watched);
        WSeat *eventSeat = getSeatForEvent(event);

        if (eventSeat && surface) {
            if (surface->type() == SurfaceWrapper::Type::XWayland) {
                auto *pointEvent = static_cast<QSinglePointEvent *>(event);
                qCDebug(lcTlXwayland) << "[XWL_POINTER_TARGET] XWayland pointer target:"
                                       << "event_type=" << event->type()
                                       << "window_id=" << xwaylandWindowId(surface)
                                       << "wrapper=" << surface
                                       << "popup_like=" << surface->isXWaylandPopupLikeTransient()
                                       << "seat=" << eventSeat
                                       << "watched=" << watched
                                       << "surfaceItem=" << surfaceItem
                                       << "local_pos=" << pointEvent->position()
                                       << "global_pos=" << pointEvent->globalPosition()
                                       << "wrapper_geometry=" << surface->geometry();
                const bool popupSequenceBelongsBeforeUpdate =
                    surface->isXWaylandPopupLikeTransient()
                    && xwaylandPointerButtonSequenceBelongsTo(eventSeat, surface);
                if (surface->isXWaylandPopupLikeTransient()) {
                    updateXWaylandPopupPointerOwnerForEvent(surface,
                                                            eventSeat,
                                                            event,
                                                            "direct-delivered");
                }
                updateXWaylandPointerButtonSequence(surface, eventSeat, event, "direct-delivered");
                if (surface->isXWaylandPopupLikeTransient()
                    && event->type() == QEvent::MouseButtonRelease) {
                    commitDeferredXWaylandPopupTransientFocus(surface,
                                                              Qt::MouseFocusReason,
                                                              eventSeat,
                                                              popupSequenceBelongsBeforeUpdate,
                                                              "direct-delivered-release");
                }
            }

            if (static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
                updateSurfaceSeatInteraction(surface, eventSeat);
                if (surface->isXWaylandPopupLikeTransient()) {
                    offerXWaylandPopupTransientFocus(surface, Qt::MouseFocusReason, eventSeat);
                    qCDebug(lcTlXwayland) << "[XWL_POPUP_MOUSE_ACTIVATE_SKIP] Skip XWayland popup activation after pointer begin:"
                                           << "window_id=" << xwaylandWindowId(surface)
                                           << "wrapper=" << surface
                                           << "parentWrapper="
                                           << xwaylandPopupTransientParentWrapper(surface, eventSeat)
                                           << "seat=" << eventSeat
                                           << "event_type=" << event->type()
                                           << "geometry=" << surface->geometry();
                } else {
                    activateSurface(surface, Qt::MouseFocusReason, eventSeat);
                }
            }
        }
    }

    return false;
}

bool Helper::unacceptedEvent(WSeat *, QWindow *, QInputEvent *event)
{
    if (!m_instance || !m_renderWindow || !m_backend)
        return false;

    if (event->isSinglePointEvent() && static_cast<QSinglePointEvent *>(event)->isBeginEvent()) {
        WSeat *eventSeat = getSeatForEvent(event);
        activateSurface(nullptr, Qt::OtherFocusReason, eventSeat);
    }

    return false;
}

bool Helper::doGesture(QInputEvent *event)
{
    if (event->type() == QEvent::NativeGesture) {
        auto e = static_cast<WGestureEvent *>(event);
        switch (e->gestureType()) {
        case Qt::BeginNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                InputDevice::instance()->processSwipeStart(e->fingerCount());

            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                InputDevice::instance()->processHoldStart(e->fingerCount());
            break;
        case Qt::EndNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture) {
                if (e->cancelled())
                    InputDevice::instance()->processSwipeCancel();
                else
                    InputDevice::instance()->processSwipeEnd();
            }
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::HoldGesture)
                InputDevice::instance()->processHoldEnd();
            break;
        case Qt::PanNativeGesture:
            if (e->libInputGestureType() == WGestureEvent::WLibInputGestureType::SwipeGesture)
                InputDevice::instance()->processSwipeUpdate(e->delta());
        case Qt::ZoomNativeGesture:
        case Qt::SmartZoomNativeGesture:
        case Qt::RotateNativeGesture:
        case Qt::SwipeNativeGesture:
        default:
            break;
        }
    }
    return false;
}

Output *Helper::createNormalOutput(WOutput *output)
{
    Output *o = Output::create(output, qmlEngine(), this);
    if (isNvidiaCardPresent()) {
        o->outputItem()->setProperty("forceSoftwareCursor", true);
    }
    o->outputItem()->stackBefore(m_rootSurfaceContainer);
    m_rootSurfaceContainer->addOutput(o);
    return o;
}

Output *Helper::createCopyOutput(WOutput *output, Output *proxy)
{
    return Output::createCopy(output, proxy, qmlEngine(), this);
}

WOutputViewport *Helper::getOwnOutputViewport(WOutput *output)
{
    // Get the output's own viewport, not screenViewport()
    // In copy mode, screenViewport() returns the primary output's viewport,
    // but we need the OutputViewport that is a direct child of the OutputItem
    Output *outputObj = getOutput(output);
    if (!outputObj || !outputObj->outputItem()) {
        qCWarning(lcTlCore) << "Invalid output object for" << output->name();
        return nullptr;
    }

    WOutputViewport *viewport = outputObj->outputItem()->findChild<WOutputViewport *>({}, Qt::FindDirectChildrenOnly);
    if (!viewport) {
        qCWarning(lcTlCore) << "No viewport found for output" << output->name()
                                << "- OutputItem may not have been fully initialized";
    }
    return viewport;
}

QList<SurfaceWrapper *> Helper::getWorkspaceSurfaces(Output *filterOutput)
{
    QList<SurfaceWrapper *> surfaces;
    WOutputRenderWindow::paintOrderItemList(
        Helper::instance()->workspace(),
        [&surfaces, filterOutput](QQuickItem *item) -> bool {
            SurfaceWrapper *surfaceWrapper = qobject_cast<SurfaceWrapper *>(item);
            if (surfaceWrapper
                && (surfaceWrapper->showOnWorkspace(
                        Helper::instance()->workspace()->current()->id())
                    && (!filterOutput || surfaceWrapper->ownsOutput() == filterOutput))) {
                surfaces.append(surfaceWrapper);
                return true;
            } else {
                return false;
            }
        });

    return surfaces;
}

void Helper::moveSurfacesToOutput(const QList<SurfaceWrapper *> &surfaces,
                                  Output *targetOutput,
                                  Output *sourceOutput)
{
    m_rootSurfaceContainer->moveSurfacesToOutput(surfaces, targetOutput, sourceOutput);
}

SurfaceWrapper *Helper::keyboardFocusSurface() const
{
    auto item = m_renderWindow->activeFocusItem();
    if (!item)
        return nullptr;
    auto surface = qobject_cast<WSurfaceItem *>(item->parent());
    if (!surface)
        return nullptr;
    return qobject_cast<SurfaceWrapper *>(surface->parent());
}

SurfaceWrapper *Helper::activatedSurface() const
{
    if (!m_rootSurfaceContainer)
        return nullptr;

    auto *seatContainer = m_rootSurfaceContainer->getSeatContainerOrDefault(m_primarySeat);
    return seatContainer ? seatContainer->activatedSurface() : nullptr;
}

void Helper::setActivatedSurface(SurfaceWrapper *newActivateSurface, WSeat *seat)
{
    if (!m_rootSurfaceContainer) {
        qCWarning(lcTlCore) << "Cannot set activated surface: root surface container is null";
        return;
    }

    WSeat *targetSeat = seat ? seat : m_primarySeat;
    auto *seatContainer = m_rootSurfaceContainer->getSeatContainerOrDefault(targetSeat);
    if (!seatContainer) {
        qCWarning(lcTlCore) << "Cannot set activated surface: no seat container for seat"
                            << targetSeat;
        return;
    }

    if (seatContainer->activatedSurface() == newActivateSurface)
        return;

    const bool isPrimarySeat = (targetSeat == m_primarySeat);
    auto *oldPrimarySurface = isPrimarySeat ? activatedSurface() : nullptr;

    if (oldPrimarySurface)
        oldPrimarySurface->setActivate(false);

    if (newActivateSurface) {
        Q_ASSERT(newActivateSurface->showOnWorkspace(workspace()->current()->id()));
        newActivateSurface->stackToLast();
        if (newActivateSurface->type() == SurfaceWrapper::Type::XWayland) {
            auto xwaylandSurface =
                qobject_cast<WXWaylandSurface *>(newActivateSurface->shellSurface());
            Q_ASSERT(!xwaylandSurface->isBypassManager());
            xwaylandSurface->restack(nullptr, WXWaylandSurface::XCB_STACK_MODE_ABOVE);
        }
    }

    if (newActivateSurface) {
        if (m_showDesktop == WindowManagementInterfaceV1::DesktopState::Show) {
            m_showDesktop = WindowManagementInterfaceV1::DesktopState::Normal;
            m_windowManagementInterfaceV1->setDesktopState(WindowManagementInterfaceV1::DesktopState::Normal);
            newActivateSurface->setHideByShowDesk(true);
        }

        Q_ASSERT(newActivateSurface->hasActiveCapability());
        workspace()->pushActivedSurface(newActivateSurface);
    }

    // SeatSurfaceManager emits activatedSurfaceChanged, and RootSurfaceContainer forwards
    // it for the primary seat. Do not emit Helper::activatedSurfaceChanged again here.
    seatContainer->setActivatedSurface(newActivateSurface, Qt::OtherFocusReason);

    if (isPrimarySeat && newActivateSurface) {
        Q_ASSERT(newActivateSurface->hasActiveCapability());
        newActivateSurface->setActivate(true);
    }

}

void Helper::onRenderWindowActiveFocusItemChanged()
{
    if (!keyboardFocusSurface()) {
        // Keyboard focus moved to a non-client window (e.g. internal QML component).
        // Notify all seats to clear the keyboard focus surface.
        const auto seats = m_seatManager->seats();
        for (auto *seat : seats) {
            if (auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(seat)) {
                if (seatContainer->keyboardFocusSurface())
                    seatContainer->setKeyboardFocusSurface(nullptr);
            }
        }
    }
}

void Helper::requestKeyboardFocus(SurfaceWrapper *wrapper, Qt::FocusReason reason, WSeat *seat)
{
    if (wrapper) {
        // Pop up through parent hierarchy until we find a non-popup surface or grabbed popup
        while (wrapper) {
            auto *popupSurface = qobject_cast<WXdgPopupSurface *>(wrapper->shellSurface());
            if (!popupSurface)
                break;
            if (popupSurface->handle()->handle()->seat)
                break;
            wrapper = wrapper->parentSurface();
        }
        if (!wrapper || !wrapper->hasFocusCapability()) {
            if (wrapper && wrapper->type() == SurfaceWrapper::Type::XWayland) {
                qCDebug(lcTlXwayland) << "[XWL_KEYBOARD_FOCUS] XWayland keyboard focus rejected:"
                                       << "window_id=" << xwaylandWindowId(wrapper)
                                       << "wrapper=" << wrapper
                                       << "reason=" << reason
                                       << "has_focus_capability="
                                       << wrapper->hasFocusCapability();
            }
            qCDebug(lcTlShell) << "Request keyboard focus for surface without focus capability!"
                                 << "surface =" << wrapper;
            return;
        }
    }

    WSeat *targetSeat = seat;
    if (!targetSeat)
        targetSeat = m_currentEventSeat ? m_currentEventSeat : getLastInteractingSeat(wrapper);
    if (!targetSeat)
        targetSeat = m_primarySeat;

    if (!targetSeat || !targetSeat->nativeHandle())
        return;

    if (wrapper && wrapper->type() == SurfaceWrapper::Type::XWayland) {
        qCDebug(lcTlXwayland) << "[XWL_KEYBOARD_FOCUS] XWayland keyboard focus request:"
                               << "window_id=" << xwaylandWindowId(wrapper)
                               << "wrapper=" << wrapper
                               << "reason=" << reason
                               << "seat=" << targetSeat
                               << "surface=" << wrapper->surface();
    }

    // Delegate to SeatSurfaceManager which handles keyboardFocusPriority checks,
    // Qt focus management, Wayland focus, multi-seat arbitration, and interaction metadata.
    auto *seatContainer = m_rootSurfaceContainer->getSeatContainer(targetSeat);
    Q_ASSERT(seatContainer);
    if (!seatContainer)
        return;
    seatContainer->setKeyboardFocusSurface(wrapper, reason);
}

void Helper::setCursorPosition(const QPointF &position)
{
    const auto seats = m_seatManager->seats();
    for (auto *seat : seats) {
        m_rootSurfaceContainer->endMoveResizeForSeat(seat);
    }
    m_primarySeat->setCursorPosition(position);
}

void Helper::handleRequestDrag([[maybe_unused]] WSurface *surface)
{
    m_primarySeat->setAlwaysUpdateHoverTarget(true);

    struct wlr_drag *drag = m_primarySeat->nativeHandle()->drag;
    Q_ASSERT(drag);
    QObject::connect(qw_drag::from(drag), &qw_drag::notify_drop, this, [this] {
        if (m_ddeShellV1)
            DDEActiveInterface::sendDrop(m_primarySeat);
    });

    QObject::connect(qw_drag::from(drag), &qw_drag::before_destroy, this, [this, drag] {
        drag->data = NULL;
        m_primarySeat->setAlwaysUpdateHoverTarget(false);
    });

    if (m_ddeShellV1)
        DDEActiveInterface::sendStartDrag(m_primarySeat);
}

void Helper::handleLockScreen(LockScreenInterface *lockScreen)
{
    connect(lockScreen, &LockScreenInterface::shutdown, this, &Helper::showShutdownMenu);
    connect(lockScreen, &LockScreenInterface::lock, this, [this]() {
        if (isNormalOrMultitaskview())
            showLockScreen(false);
    });
    connect(lockScreen, &LockScreenInterface::switchUser, this, &Helper::showSwitchUser);
}


void Helper::onSessionNew(const QString &sessionId, const QDBusObjectPath &sessionPath)
{
    const auto path = sessionPath.path();
    qCDebug(lcTlCore) << "Session new, sessionId:" << sessionId << ", sessionPath:" << path;
    QDBusConnection::systemBus().connect("org.freedesktop.login1", path, "org.freedesktop.login1.Session", "Lock", this, SLOT(onSessionLock()));
    QDBusConnection::systemBus().connect("org.freedesktop.login1", path, "org.freedesktop.login1.Session", "Unlock", this, SLOT(onSessionUnlock()));
}

void Helper::onSessionLock()
{
    showLockScreen();
}

void Helper::onSessionUnlock()
{
    if (m_lockScreen) {
        m_lockScreen->unlock();
    }
}

void Helper::onExtSessionLock(WSessionLock *lock)
{
#ifdef EXT_SESSION_LOCK_V1
    if (m_lockScreen->isLocked()) {
        lock->finish();
        return;
    }

    m_lockScreen->onExternalLock(lock);

    prepareLockScreenTransition();

    lock->safeConnect(&WSessionLock::abandoned, this, [this]() {
        m_lockScreenGraceTimer->stop();
        setNoAnimation(false);
    });

    lock->safeConnect(&WSessionLock::canceled, this, [this]() {
        m_lockScreenGraceTimer->stop();
    });

    m_lockScreenGraceTimer->disconnect();
    // grace 300ms for possible client to
    connect(m_lockScreenGraceTimer, &QTimer::timeout, this, [this, lock]() {
        setNoAnimation(true);
        lock->lock();
    });
    m_lockScreenGraceTimer->start();
#endif
}

void Helper::allowNonDrmOutputAutoChangeMode(WOutput *output)
{
    output->safeConnect(&qw_output::notify_request_state,
                        this,
                        [this](wlr_output_event_request_state *newState) {
                            if (newState->state->committed & WLR_OUTPUT_STATE_MODE) {
                                auto output = qobject_cast<qw_output *>(sender());
                                if (!output->commit_state(newState->state)) {
                                    qCCritical(lcTlCore, "commit failed on output %s",
                                               output->handle()->name);
                                }
                            }
                        });
}

int Helper::indexOfOutput(WOutput *output) const
{
    for (int i = 0; i < m_outputList.size(); i++) {
        if (m_outputList.at(i)->output() == output)
            return i;
    }
    return -1;
}

Output *Helper::getOutput(WOutput *output) const
{
    for (auto o : std::as_const(m_outputList)) {
        if (o->output() == output)
            return o;
    }
    return nullptr;
}

void Helper::addOutput()
{
    qobject_cast<qw_multi_backend *>(m_backend->handle())
        ->for_each_backend(
            [](wlr_backend *backend, void *) {
                if (auto x11 = qw_x11_backend::from(backend)) {
                    qw_output::from(x11->output_create());
                } else if (auto wayland = qw_wayland_backend::from(backend)) {
                    qw_output::from(wayland->output_create());
                }
            },
            nullptr);
}

void Helper::setOutputMode(OutputMode mode)
{
    if (m_outputList.length() < 2 || m_mode == mode)
        return;
    m_mode = mode;
    Q_EMIT outputModeChanged();
    for (int i = 0; i < m_outputList.size(); i++) {
        if (m_outputList.at(i) == m_rootSurfaceContainer->primaryOutput())
            continue;
        Output *o = nullptr;
        if (mode == OutputMode::Copy) {
            o = createCopyOutput(m_outputList.at(i)->output(),
                                 m_rootSurfaceContainer->primaryOutput());
            m_rootSurfaceContainer->removeOutput(m_outputList.at(i));
        } else if (mode == OutputMode::Extension) {
            o = createNormalOutput(m_outputList.at(i)->output());
            o->enable();
        }
        m_outputList.at(i)->deleteLater();
        m_outputList.replace(i, o);
    }
}

float Helper::animationSpeed() const
{
    return m_animationSpeed;
}

void Helper::setAnimationSpeed(float newAnimationSpeed)
{
    if (qFuzzyCompare(m_animationSpeed, newAnimationSpeed))
        return;
    m_animationSpeed = newAnimationSpeed;
    Q_EMIT animationSpeedChanged();
}

Helper::OutputMode Helper::outputMode() const
{
    return m_mode;
}

/**
 * Add a WSocket to the Wayland server.
 * This function is used by Treeland::ActivateWayland.
 *
 * @param socket WSocket to add
 */
void Helper::addSocket(WSocket *socket)
{
    m_server->addSocket(socket);
}

bool Helper::toggleDebugMenuBar()
{
    bool ok = false;

    const auto outputs = rootSurfaceContainer()->outputs();
    if (outputs.isEmpty())
        return false;

    bool firstOutputDebugMenuBarIsVisible = false;
    if (auto menuBar = outputs.first()->debugMenuBar()) {
        firstOutputDebugMenuBarIsVisible = menuBar->isVisible();
    }

    for (const auto &output : outputs) {
        if (output->debugMenuBar()) {
            output->debugMenuBar()->setVisible(!firstOutputDebugMenuBarIsVisible);
            ok = true;
        }
    }

    return ok;
}

WindowManagementInterfaceV1::DesktopState Helper::showDesktopState() const
{
    return m_showDesktop;
}

bool Helper::isLaunchpad(WLayerSurface *surface) const
{
    if (!surface) {
        return false;
    }

    auto scope = QString(surface->handle()->handle()->scope);

    return scope == "dde-shell/launchpad";
}

void Helper::setLaunchpadMapped(WOutput *output, bool mapped)
{
    Q_EMIT launchpadMappedChanged(output, mapped);
}

void Helper::showDesktop(WOutput *output)
{
    Q_EMIT showDesktopRequested(output);
}

void Helper::startLockscreen(WOutput *output, bool showAnimation)
{
    Q_EMIT startLockscreened(output, showAnimation);
}

QString Helper::currentWorkspaceWallpaper(WOutput *output)
{
    return m_wallpaperManager->currentWorkspaceWallpaper(output);
}

QString Helper::currentLockScreenWallpaper(WOutput *output)
{
    return m_wallpaperManager->currentLockScreenWallpaper(output);
}

void Helper::handleWindowPicker(WindowPickerInterface *picker)
{
    connect(picker, &WindowPickerInterface::pick, this, [this, picker](const QString &hint) {
        auto windowPicker =
            qobject_cast<WindowPicker *>(qmlEngine()->createWindowPicker(m_rootSurfaceContainer));
        windowPicker->setHint(hint);
        connect(windowPicker,
                &WindowPicker::windowPicked,
                this,
                [picker, windowPicker](WSurfaceItem *surfaceItem) {
                    if (surfaceItem) {
                        auto credentials = WClient::getCredentials(
                            surfaceItem->surface()->waylandClient()->handle());
                        picker->sendWindowPid(credentials->pid);
                        windowPicker->deleteLater();
                    }
                });
        connect(picker,
                &WindowPickerInterface::beforeDestroy,
                windowPicker,
                &WindowPicker::deleteLater);
    });
}

void Helper::setMultitaskViewImpl(IMultitaskView *impl)
{
    m_multitaskView = impl;
}

void Helper::setLockScreenImpl(ILockScreen *impl)
{
#if !defined(DISABLE_DDM) || defined(EXT_SESSION_LOCK_V1)
    if (!impl) {
        if (m_lockScreen) {
            m_lockScreen = nullptr;
            delete m_lockScreen;
        }
        return;
    }

    m_lockScreen = new LockScreen(impl, m_rootSurfaceContainer, m_greeterProxy);
    m_lockScreen->setZ(RootSurfaceContainer::LockScreenZOrder);
    m_lockScreen->setObjectName(QStringLiteral("LockScreenContainer"));
    m_lockScreen->setVisible(false);

    m_greeterProxy->setLockScreen(m_lockScreen);

    for (auto *output : std::as_const(m_rootSurfaceContainer->outputs())) {
        m_lockScreen->addOutput(output);
    }

    if (auto primaryOutput = m_rootSurfaceContainer->primaryOutput()) {
        m_lockScreen->setPrimaryOutputName(primaryOutput->output()->name());
    }

    connect(m_lockScreen, &LockScreen::unlock, this, [this] {
        setCurrentMode(CurrentMode::Normal);
        setWorkspaceVisible(true);
#ifdef EXT_SESSION_LOCK_V1
        setNoAnimation(false);
#endif
        if (auto *surface = activatedSurface()) {
            if (surface->hasFocusCapability()) {
                requestKeyboardFocus(surface, Qt::NoFocusReason);
            }
        }
    });
    if (!impl) {
        return;
    }
    if (CmdLine::ref().useLockScreen()) {
        showLockScreen(false);
    }
#else
    Q_UNUSED(impl)
#endif
}

void Helper::setCurrentMode(CurrentMode mode)
{
    if (m_currentMode == mode)
        return;

    setBlockActivateSurface(mode != CurrentMode::Normal);

    m_currentMode = mode;

    Q_EMIT currentModeChanged();
}

void Helper::prepareLockScreenTransition()
{
    if (m_multitaskView) {
        m_multitaskView->immediatelyExit();
    }
    deleteTaskSwitch();
    setCurrentMode(CurrentMode::LockScreen);
    setWorkspaceVisible(false);
}

void Helper::showLockScreen(bool switchToGreeter)
{
    if (!isLockScreenAvailable()) {
        return;
    }
    if (m_lockScreen->isLocked()) {
        return;
    }

    prepareLockScreenTransition();
    m_lockScreen->lock();

    if (switchToGreeter) {
        QThreadPool::globalInstance()->start([]() {
            QDBusInterface interface("org.freedesktop.DisplayManager",
                                     "/org/freedesktop/DisplayManager/Seat0",
                                     "org.freedesktop.DisplayManager.Seat",
                                     QDBusConnection::systemBus());
            interface.call("SwitchToGreeter");
        });
    }
}

bool Helper::isLockScreenAvailable() const
{
    return m_lockScreen && m_lockScreen->available();
}

void Helper::showShutdownMenu()
{
    if (!isLockScreenAvailable() || !isNormalOrMultitaskview()) {
        return;
    }

    prepareLockScreenTransition();
    m_lockScreen->shutdown();
}

void Helper::showSwitchUser()
{
    if (!isLockScreenAvailable() || !isNormalOrMultitaskview()) {
        return;
    }

    prepareLockScreenTransition();
    m_lockScreen->switchUser();
}

WSeat *Helper::seat() const
{
    return m_primarySeat;
}

void Helper::handleLeftButtonStateChanged(const QInputEvent *event)
{
    Q_ASSERT(m_ddeShellV1 && m_primarySeat);
    const QMouseEvent *me = static_cast<const QMouseEvent *>(event);
    if (me->button() == Qt::LeftButton) {
        if (event->type() == QEvent::MouseButtonPress) {
            DDEActiveInterface::sendActiveIn(DDEActiveInterface::Mouse, m_primarySeat);
        } else {
            DDEActiveInterface::sendActiveOut(DDEActiveInterface::Mouse, m_primarySeat);
        }
    }
}

void Helper::handleWhellValueChanged(const QInputEvent *event)
{
    Q_ASSERT(m_ddeShellV1 && m_primarySeat);
    const QWheelEvent *we = static_cast<const QWheelEvent *>(event);
    QPoint delta = we->angleDelta();
    if (delta.x() + delta.y() < 0) {
        DDEActiveInterface::sendActiveOut(DDEActiveInterface::Wheel, m_primarySeat);
    }
    if (delta.x() + delta.y() > 0) {
        DDEActiveInterface::sendActiveIn(DDEActiveInterface::Wheel, m_primarySeat);
    }
}

void Helper::restoreFromShowDesktop(SurfaceWrapper *activeSurface)
{
    if (m_showDesktop == WindowManagementInterfaceV1::DesktopState::Show) {
        m_showDesktop = WindowManagementInterfaceV1::DesktopState::Normal;
        m_windowManagementInterfaceV1->setDesktopState(WindowManagementInterfaceV1::DesktopState::Normal);
        if (activeSurface) {
            activeSurface->restoreFromMinimized();
        }
        const auto &surfaces = getWorkspaceSurfaces();
        for (auto &surface : surfaces) {
            if (!surface->isMinimized() && !surface->isVisible()) {
                surface->setHideByShowDesk(true);
                surface->setSurfaceState(SurfaceWrapper::State::Minimized);
            }
        }
    }
}

Output *Helper::getOutputAtCursor() const
{
    QPoint cursorPos = QCursor::pos();
    for (auto output : std::as_const(m_outputList)) {
        QRectF outputGeometry(output->outputItem()->position(), output->outputItem()->size());
        if (outputGeometry.contains(cursorPos)) {
            return output;
        }
    }

    return m_rootSurfaceContainer->primaryOutput();
}

void Helper::handleNewForeignToplevelCaptureRequest(wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request)
{
    if (!request || !request->toplevel_handle) {
        qCWarning(lcTlCapture) << "Invalid capture request or toplevel handle";
        return;
    }

    auto *qw_handle = qw_ext_foreign_toplevel_handle_v1::from(request->toplevel_handle);
    WToplevelSurface *toplevelSurface = m_extForeignToplevelListV1->findSurfaceByHandle(qw_handle);
    if (!toplevelSurface) {
        qCWarning(lcTlCapture) << "Could not find toplevel surface for handle";
        return;
    }

    SurfaceWrapper *surfaceWrapper = m_rootSurfaceContainer->getSurface(toplevelSurface);
    if (!surfaceWrapper) {
        qCWarning(lcTlCapture) << "Could not find SurfaceWrapper for toplevel surface";
        return;
    }

    WSurfaceItem *surfaceItem = surfaceWrapper->surfaceItem();
    if (!surfaceItem) {
        qCWarning(lcTlCapture) << "Could not get WSurfaceItem from SurfaceWrapper";
        return;
    }

    WSurfaceItemContent *surfaceContent = surfaceItem->findItemContent();
    if (!surfaceContent) {
        qCWarning(lcTlCapture) << "Could not find WSurfaceItemContent";
        return;
    }

    qCDebug(lcTlCapture) << "Found WSurfaceItemContent for capture:"
             << "size=" << surfaceContent->size()
             << "implicitSize=" << QSizeF(surfaceContent->implicitWidth(), surfaceContent->implicitHeight())
             << "isTextureProvider=" << surfaceContent->isTextureProvider();

    auto *output = surfaceWrapper->ownsOutput()->output();
    if (!output) {
        qCWarning(lcTlCapture) << "Could not get WOutput from SurfaceWrapper";
        return;
    }

    auto *imageCaptureSource = new WExtImageCaptureSourceV1Impl(surfaceContent, output);

    bool success = qw_ext_foreign_toplevel_image_capture_source_manager_v1::request_accept(
        request, *imageCaptureSource);

    if (!success) {
        qCWarning(lcTlCapture) << "Failed to accept foreign toplevel image capture request";
        delete imageCaptureSource;
    }
}

DDMInterfaceV1 *Helper::ddmInterfaceV1() const {
    return m_ddmInterfaceV1;
}

void Helper::activateSession() {
    if (!m_backend->isSessionActive())
        m_backend->activateSession();
}

bool Helper::activateUserSession(const QString &username, int sessionId)
{
    if (!m_userModel->getUser(username))
        return false;

    const auto update = m_sessionManager->prepareActiveUserSession(username, sessionId);
    if (!update)
        return false;

    m_userModel->setCurrentUserName(username);
    m_sessionManager->commitActiveUserSession(update);
    return true;
}

void Helper::deactivateSession() {
    if (m_backend->isSessionActive())
        m_backend->deactivateSession();
}

void Helper::enableRender() {
    m_renderWindow->setRenderEnabled(true);
}

void Helper::disableRender() {
    m_renderWindow->setRenderEnabled(false);
}

void Helper::setBlockActivateSurface(bool block)
{
    if (block == m_blockActivateSurface)
        return;
    m_blockActivateSurface = block;
    Q_EMIT blockActivateSurfaceChanged();
}

bool Helper::blockActivateSurface() const
{
    return m_blockActivateSurface;
}

bool Helper::noAnimation() const {
    return m_noAnimation;
}

void Helper::setNoAnimation(bool noAnimation) {
    if (m_noAnimation == noAnimation)
        return;
    m_noAnimation = noAnimation;
    emit noAnimationChanged();
}

void Helper::toggleFpsDisplay()
{
    if (m_fpsDisplay) {
        m_fpsDisplay->deleteLater();
        m_fpsDisplay = nullptr;
        return;
    }

    m_fpsDisplay = qmlEngine()->createFpsDisplay(m_renderWindow->contentItem());
}

void Helper::applyCopyModeToOutputs(Output *primaryOutput, const QList<SurfaceWrapper *> &surfaces)
{
    Q_ASSERT(primaryOutput);

    // Convert existing outputs to copy outputs
    for (int i = 0; i < m_outputList.size(); i++) {
        Output *existingOutput = m_outputList.at(i);

        if (existingOutput == primaryOutput) {
            continue;
        }

        Output *copyOutput = createCopyOutput(existingOutput->output(), primaryOutput);
        m_rootSurfaceContainer->removeOutput(existingOutput);
        existingOutput->deleteLater();
        m_outputList.replace(i, copyOutput);
        m_rootSurfaceContainer->addOutput(copyOutput);
        copyOutput->enable();
    }

    m_mode = OutputMode::Copy;
    Q_EMIT outputModeChanged();

    if (!surfaces.isEmpty()) {
        moveSurfacesToOutput(surfaces, primaryOutput, nullptr);
    }

    if (m_outputConfigState) {
        m_outputConfigState->clearCopyModeIntent();
    }
}

void Helper::restoreCopyMode()
{
    Output *primaryOutput = m_rootSurfaceContainer->primaryOutput();
    if (!primaryOutput) {
        qCWarning(lcTlCore) << "Cannot restore Copy Mode: no primary output available";
        return;
    }

    const auto &allSurfaces = getWorkspaceSurfaces();
    applyCopyModeToOutputs(primaryOutput, allSurfaces);
}

/**
 * Move a XWayland window's surface corresponding to wid, to a
 * position relative to a WSurface. Top-left point is always used.
 *
 * @param wid X Window ID for the XWayland surface
 * @param anchor The anchor WSurface to be relative to
 * @param dx Horizontal distance between the top-left point of anchor and the destination
 * @param dy Vertical distance between the top-left point of anchor and the destination
 */
bool Helper::setXWindowPositionRelative(uint wid, WSurface *anchor, wl_fixed_t dx, wl_fixed_t dy) const
{
    SurfaceWrapper *ach = m_rootSurfaceContainer->getSurface(anchor);
    if (!ach) {
        qCWarning(lcTlCore) << "setXWindowPositionRelative: Failed to get SurfaceWrapper from WSurface";
        return false;
    }

    SurfaceWrapper *target = nullptr;
    for (SurfaceWrapper *wrapper : std::as_const(rootSurfaceContainer()->surfaces())) {
        if (wrapper->type() == SurfaceWrapper::Type::XWayland) {
            wlr_xwayland_surface *surface =
                wlr_xwayland_surface_try_from_wlr_surface(wrapper->surface()->handle()->handle());
            if (surface && surface->window_id == static_cast<xcb_window_t>(wid)) {
                target = wrapper;
                break;
            }
        }
    }
    if (!target) {
        qCWarning(lcTlCore) << "setXWindowPositionRelative: XWayland surface corresponding to WID" << wid << "not found!";
        return false;
    }

    QRectF rect(ach->position(), target->size());
    rect.translate(wl_fixed_to_double(dx), wl_fixed_to_double(dy));

    // For XWayland surfaces, setting wrapper position while following
    // implicit surface position may get overwritten by feedback updates.
    // Temporarily switch to compositor-driven position so moveTo() sends
    // configure with the new coordinates.
    target->setXwaylandPositionFromSurface(false);
    target->setPosition(rect.topLeft());
    target->setXwaylandPositionFromSurface(true);
    return true;
}

WXWayland *Helper::createXWayland()
{
    return shellHandler()->createXWayland(m_server, m_primarySeat, m_compositor, false);
}

WSeat *Helper::findSeatForSurface(SurfaceWrapper *wrapper) const
{
    return getLastInteractingSeat(wrapper);
}

void Helper::handleRequestDragForSeat(WSeat *seat, WSurface *)
{
    if (!seat || !seat->nativeHandle())
        return;

    seat->setAlwaysUpdateHoverTarget(true);
    struct wlr_drag *drag = seat->nativeHandle()->drag;
    Q_ASSERT(drag);

    QObject::connect(qw_drag::from(drag), &qw_drag::notify_drop, this, [this, seat] {
        if (m_ddeShellV1)
            DDEActiveInterface::sendDrop(seat);
    });

    QObject::connect(qw_drag::from(drag), &qw_drag::before_destroy, this, [seat, drag] {
        drag->data = NULL;
        seat->setAlwaysUpdateHoverTarget(false);
    });

    if (m_ddeShellV1)
        DDEActiveInterface::sendStartDrag(seat);
}

WSeat *Helper::getLastInteractingSeat(SurfaceWrapper *surface) const
{
    if (!surface) {
        return nullptr;
    }

    auto lastSeatVariant = surface->property("lastInteractingSeat");
    if (lastSeatVariant.isValid()) {
        auto seat = lastSeatVariant.value<WSeat*>();
        if (m_seatManager->seats().contains(seat)) {
            return seat;
        }
    }
    return nullptr;
}

void Helper::updateSurfaceSeatInteraction(SurfaceWrapper *surface, WSeat *seat)
{
    if (!surface || !seat)
        return;

    surface->setProperty("lastInteractingSeat", QVariant::fromValue(seat));
    surface->setProperty("lastInteractionTime", QDateTime::currentMSecsSinceEpoch());
}

void Helper::switchWorkspaceForSeat(WSeat *seat, int index)
{
    if (!seat)
        return;
    workspace()->switchTo(index);
}
