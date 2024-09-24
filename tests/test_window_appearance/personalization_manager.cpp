// Copyright (C) 2024 justforlxz <zhangdingyuan@deepin.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "personalization_manager.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

PersonalizationManager::PersonalizationManager()
    : QWaylandClientExtensionTemplate<PersonalizationManager>(1)
{
}

Appearance::Appearance(struct ::treeland_personalization_appearance_context_v1 *object)
    : QWaylandClientExtensionTemplate<Appearance>(1)
    , QtWayland::treeland_personalization_appearance_context_v1(object)
{
}

void Appearance::treeland_personalization_appearance_context_v1_round_corner_radius(int32_t radius)
{
}

void Appearance::treeland_personalization_appearance_context_v1_font(const QString &font_name) { }

void Appearance::treeland_personalization_appearance_context_v1_monospace_font(
    const QString &font_name)
{
}

void Appearance::treeland_personalization_appearance_context_v1_cursor_theme(
    const QString &theme_name)
{
    Q_EMIT cursorThemeChanged(theme_name);
}

void Appearance::treeland_personalization_appearance_context_v1_icon_theme(
    const QString &theme_name)
{
}

QT_END_NAMESPACE
