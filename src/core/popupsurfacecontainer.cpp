// Copyright (C) 2025-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "popupsurfacecontainer.h"

#include "surface/surfacewrapper.h"
#include "common/treelandlogging.h"

#include <QLoggingCategory>

#include <wxdgpopupsurface.h>
#include <qwxdgshell.h>

#include <wayland-util.h>

extern "C" {
#include <wlr/types/wlr_xdg_shell.h>
}

/**
 * Check if a popup has grabbed input.
 * 
 * In Wayland, popups that call xdg_popup.grab() are "real" popups (menus, dropdowns, etc.)
 * that should be dismissed when clicking outside. Popups without grab (like tooltips)
 * should NOT be dismissed by the compositor.
 */
static bool popupHasGrab(SurfaceWrapper *surface)
{
    if (!surface || surface->type() != SurfaceWrapper::Type::XdgPopup)
        return false;

    auto shellSurface = surface->shellSurface();
    if (!shellSurface)
        return false;

    auto popupSurface = qobject_cast<WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface *>(shellSurface);
    if (!popupSurface)
        return false;

    auto handle = popupSurface->handle();
    if (!handle)
        return false;

    auto wlrPopup = handle->handle();
    if (!wlrPopup)
        return false;

    // Check if the popup has grabbed input by testing if grab_link is non-empty
    // wl_list_empty() returns true when grab_link.next == &grab_link (self-referential)
    return !wl_list_empty(&wlrPopup->grab_link);
}

PopupSurfaceContainer::PopupSurfaceContainer(SurfaceContainer *parent)
    : SurfaceContainer(parent)
{
    setAcceptedMouseButtons(Qt::AllButtons);
}

void PopupSurfaceContainer::mousePressEvent(QMouseEvent *event)
{
    // Filter surfaces to only include XdgPopup type surfaces that have grabbed input
    // Only these popups should be dismissed when clicking outside
    QList<SurfaceWrapper *> grabbedPopupSurfaces;
    for (auto surface : std::as_const(surfaces())) {
        if (popupHasGrab(surface)) {
            grabbedPopupSurfaces.append(surface);
        }
    }

    if (!grabbedPopupSurfaces.isEmpty()) {
        qCDebug(treelandShell) << "Intercepting mouse press event due to active grabbed popup surfaces";
        // If surfaces are not empty, intercept the mouse press event to prevent it from reaching
        // lower z-index components
        event->accept();
        // Get all surfaces in this container and close them
        for (auto surface : grabbedPopupSurfaces) {
            // Maybe the surface is removed by it's parent surface
            if (!surfaces().contains(surface)) {
                continue;
            }
            if (!surface->shellSurface()) {
                qCCritical(treelandShell) << "Ignore invalid popup surface:" << surface;
                continue;
            }
            surface->requestClose();
        }
        return;
    }

    SurfaceContainer::mousePressEvent(event);
}
