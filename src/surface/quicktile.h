// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#pragma once

#include <QRectF>
#include <QSizeF>
#include <qglobal.h>

class Output;
class SurfaceWrapper;

namespace QuickTile {
enum class Mode
{
    None,
    Left,
    Right,
    Maximize,
};

QRectF geometry(Mode mode, Output *output);

void apply(SurfaceWrapper *surface, Mode mode, Output *output);

// Instantly restore `surface` to Normal
void cancel(SurfaceWrapper *surface);
} // namespace QuickTile
