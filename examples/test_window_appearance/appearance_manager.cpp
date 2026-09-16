// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "appearance_manager.h"

QT_BEGIN_NAMESPACE

AppearanceObserver::AppearanceObserver()
    : QWaylandClientExtensionTemplate<AppearanceObserver>(1)
{
}

void AppearanceObserver::treeland_appearance_v1_cursor_theme(const QString &name)
{
    Q_EMIT cursorThemeChanged(name);
}

void AppearanceObserver::treeland_appearance_v1_cursor_size(uint32_t width, uint32_t height)
{
    Q_EMIT cursorSizeChanged(width, height);
}

void AppearanceObserver::treeland_appearance_v1_font(const QString &font_name)
{
    Q_EMIT fontChanged(font_name);
}

void AppearanceObserver::treeland_appearance_v1_monospace_font(const QString &font_name)
{
    Q_EMIT monospaceFontChanged(font_name);
}

void AppearanceObserver::treeland_appearance_v1_font_size(uint32_t size)
{
    Q_EMIT fontSizeChanged(size);
}

void AppearanceObserver::treeland_appearance_v1_icon_theme(const QString &theme_name)
{
    Q_EMIT iconThemeChanged(theme_name);
}

void AppearanceObserver::treeland_appearance_v1_accent_color(uint32_t r, uint32_t g, uint32_t b)
{
    Q_EMIT accentColorChanged(r, g, b);
}

void AppearanceObserver::treeland_appearance_v1_window_opacity(wl_fixed_t opacity)
{
    Q_EMIT windowOpacityChanged(opacity);
}

void AppearanceObserver::treeland_appearance_v1_color_scheme(uint32_t scheme)
{
    Q_EMIT colorSchemeChanged(scheme);
}

void AppearanceObserver::treeland_appearance_v1_titlebar_height(uint32_t height)
{
    Q_EMIT titlebarHeightChanged(height);
}

void AppearanceObserver::treeland_appearance_v1_corner_radius(uint32_t radius)
{
    Q_EMIT cornerRadiusChanged(radius);
}

AppearanceManager::AppearanceManager()
    : QWaylandClientExtensionTemplate<AppearanceManager>(1)
{
}

QT_END_NAMESPACE
