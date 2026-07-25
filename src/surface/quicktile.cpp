// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "surface/quicktile.h"

#include "output/output.h"
#include "surface/surfacewrapper.h"

namespace QuickTile {

QRectF geometry(Mode mode, Output *output)
{
    if (mode == Mode::None || !output)
        return QRectF();

    const QRectF area = output->validGeometry();
    switch (mode) {
    case Mode::Left:
        return QRectF(area.topLeft(), QSizeF(area.width() / 2, area.height()));
    case Mode::Right:
        return QRectF(QPointF(area.left() + area.width() / 2, area.top()),
                      QSizeF(area.width() / 2, area.height()));
    case Mode::Maximize:
        return area;
    case Mode::None:
        break;
    }
    return QRectF();
}

void apply(SurfaceWrapper *surface, Mode mode, Output *output)
{
    if (!surface)
        return;

    if (mode == Mode::None) {
        cancel(surface);
        return;
    }

    if (mode == Mode::Maximize) {
        // Use the cursor's output geometry, not the surface's current output,
        // so maximize lands on the screen the user dragged to (matches preview).
        if (output)
            surface->setMaximizedGeometry(output->validGeometry());
        surface->maximize();
        return;
    }

    const QRectF geo = geometry(mode, output);
    if (!geo.isValid())
        return;

    // Order matters: setTilingGeometry first so that a subsequent
    // setSurfaceState(Tiling) reads the new m_tilingGeometry as its target.
    surface->setTilingGeometry(geo);
    surface->setSurfaceState(SurfaceWrapper::State::Tiling);
}

void cancel(SurfaceWrapper *surface)
{
    if (!surface)
        return;

    surface->setSurfaceStateDirectly(SurfaceWrapper::State::Normal);
}

} // namespace QuickTile
