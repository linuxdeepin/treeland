// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wallpapercontroller.h"

static QMap<Waylib::Server::WOutput *, WallpaperController *> S_OUTPUTS;

WallpaperController::WallpaperController(QQuickItem *parent)
    : QQuickItem(parent)
{
}

WallpaperController::~WallpaperController()
{
    S_OUTPUTS.remove(m_output);
}

void WallpaperController::setSource(const QString &source)
{
    if (m_source == source) {
        return;
    }

    m_source = source;

    Q_EMIT sourceChanged();
}

void WallpaperController::setAnimationType(AnimationType type)
{
    if (m_animationType == type) {
        return;
    }

    m_animationType = type;

    Q_EMIT animationTypeChanged();
}

void WallpaperController::setOutput(Waylib::Server::WOutput *output)
{
    if (!output) {
        return;
    }

    if (m_output == output) {
        return;
    }

    if (S_OUTPUTS.contains(output)) {
        return;
    }

    m_output = output;

    S_OUTPUTS[output] = this;

    Q_EMIT outputChanged();
}

WallpaperController *WallpaperController::get(Waylib::Server::WOutput *output)
{
    return S_OUTPUTS[output];
}
