// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "popupsurfacecontainer.h"

#include "surface/surfacewrapper.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(qLcPopupContainer, "treeland.shell.popupContainer")

PopupSurfaceContainer::PopupSurfaceContainer(SurfaceContainer *parent)
    : SurfaceContainer(parent)
{
    setAcceptedMouseButtons(Qt::AllButtons);
}

void PopupSurfaceContainer::mousePressEvent(QMouseEvent *event)
{
    // Filter surfaces to only include XdgPopup type surfaces
    QList<SurfaceWrapper *> xdgPopupSurfaces;
    for (auto surface : std::as_const(surfaces())) {
        if (surface->type() == SurfaceWrapper::Type::XdgPopup) {
            xdgPopupSurfaces.append(surface);
        }
    }

    if (!xdgPopupSurfaces.isEmpty()) {
        qCDebug(qLcPopupContainer) << "Intercepting mouse press event due to active popup surfaces";
        // If surfaces are not empty, intercept the mouse press event to prevent it from reaching
        // lower z-index components
        event->accept();
        // Get all surfaces in this container and close them
        for (auto surface : xdgPopupSurfaces) {
            // Maybe the surface is removed by it's parent surface
            if (!surfaces().contains(surface)) {
                continue;
            }
            if (!surface->shellSurface()) {
                qCCritical(qLcPopupContainer) << "Ignore invalid popup surface:" << surface;
                continue;
            }
            surface->requestClose();
        }
        return;
    }

    SurfaceContainer::mousePressEvent(event);
}
