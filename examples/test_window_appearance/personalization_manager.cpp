// Copyright (C) 2024 justforlxz <zhangdingyuan@deepin.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalization_manager.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

PersonalizationManager::PersonalizationManager()
    : QWaylandClientExtensionTemplate<PersonalizationManager>(1)
{
}

Cursor::Cursor(struct ::treeland_personalization_cursor_context_v1 *object)
    : QWaylandClientExtensionTemplate<Cursor>(1)
    , QtWayland::treeland_personalization_cursor_context_v1(object)

{
}

void Cursor::treeland_personalization_cursor_context_v1_verfity(int32_t success)
{
    Q_EMIT verifyChanged(success);
}

void Cursor::treeland_personalization_cursor_context_v1_theme(const QString &name)
{
    Q_EMIT themeChanged(name);
}

void Cursor::treeland_personalization_cursor_context_v1_size(uint32_t size)
{
    Q_EMIT sizeChanged(size);
}

Font::Font(struct ::treeland_personalization_font_context_v1 *object)
    : QWaylandClientExtensionTemplate<Font>(1)
    , QtWayland::treeland_personalization_font_context_v1(object)

{
}

void Font::treeland_personalization_font_context_v1_font(const QString &font_name)
{
    Q_EMIT fontChanged(font_name);
}

void Font::treeland_personalization_font_context_v1_monospace_font(const QString &font_name)
{
    Q_EMIT monoFontChanged(font_name);
}

void Font::treeland_personalization_font_context_v1_font_size(uint32_t font_size)
{
    Q_EMIT fontSizeChanged(font_size);
}

Appearance::Appearance(struct ::treeland_personalization_appearance_context_v1 *object)
    : QWaylandClientExtensionTemplate<Appearance>(1)
    , QtWayland::treeland_personalization_appearance_context_v1(object)
{
}

void Appearance::treeland_personalization_appearance_context_v1_round_corner_radius(int32_t radius)
{
    Q_EMIT radiusChanged(radius);
}

void Appearance::treeland_personalization_appearance_context_v1_icon_theme(
    const QString &theme_name)
{
    Q_EMIT iconThemeChanged(theme_name);
}

void Appearance::treeland_personalization_appearance_context_v1_active_color(
    const QString &active_color)
{
    Q_EMIT activeColorChanged(active_color);
}

void Appearance::treeland_personalization_appearance_context_v1_window_opacity(uint32_t opacity)
{
    Q_EMIT windowOpacityChanged(opacity);
}

void Appearance::treeland_personalization_appearance_context_v1_window_theme_type(uint32_t type)
{
    Q_EMIT windowThemeTypeChanged(type);
}

void Appearance::treeland_personalization_appearance_context_v1_window_titlebar_height(
    uint32_t height)
{
    Q_EMIT windowTitlebarHeightChanged(height);
}

QT_END_NAMESPACE
