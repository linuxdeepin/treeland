// Copyright (C) 2024 justforlxz <zhangdingyuan@deepin.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef CUSTOMEXTENSION_H
#define CUSTOMEXTENSION_H

#include "qwayland-treeland-personalization-manager-v1.h"

#include <QtGui/QWindow>
#include <QtWaylandClient/QWaylandClientExtension>

QT_BEGIN_NAMESPACE

class PersonalizationManager
    : public QWaylandClientExtensionTemplate<PersonalizationManager>
    , public QtWayland::treeland_personalization_manager_v1
{
    Q_OBJECT
public:
    explicit PersonalizationManager();
};

class Cursor
    : public QWaylandClientExtensionTemplate<Cursor>
    , public QtWayland::treeland_personalization_cursor_context_v1
{
    Q_OBJECT
public:
    explicit Cursor(struct ::treeland_personalization_cursor_context_v1 *object);

Q_SIGNALS:
    void verifyChanged(int32_t success);
    void themeChanged(const QString &name);
    void sizeChanged(uint32_t size);

protected:
    void treeland_personalization_cursor_context_v1_verfity(int32_t success) override;
    void treeland_personalization_cursor_context_v1_theme(const QString &name) override;
    void treeland_personalization_cursor_context_v1_size(uint32_t size) override;
};

class Font
    : public QWaylandClientExtensionTemplate<Font>
    , public QtWayland::treeland_personalization_font_context_v1
{
    Q_OBJECT
public:
    explicit Font(struct ::treeland_personalization_font_context_v1 *object);

Q_SIGNALS:
    void fontChanged(const QString &font_name);
    void monoFontChanged(const QString &font_name);
    void fontSizeChanged(uint32_t font_size);

protected:
    void treeland_personalization_font_context_v1_font(const QString &font_name) override;
    void treeland_personalization_font_context_v1_monospace_font(const QString &font_name) override;
    void treeland_personalization_font_context_v1_font_size(uint32_t font_size) override;
};

class Appearance
    : public QWaylandClientExtensionTemplate<Appearance>
    , public QtWayland::treeland_personalization_appearance_context_v1
{
    Q_OBJECT
public:
    explicit Appearance(struct ::treeland_personalization_appearance_context_v1 *object);

Q_SIGNALS:
    void radiusChanged(int32_t radius);
    void iconThemeChanged(const QString &name);
    void activeColorChanged(const QString &color);
    void windowOpacityChanged(uint32_t opacity);
    void windowThemeTypeChanged(uint32_t type);
    void windowTitlebarHeightChanged(uint32_t height);

protected:
    void treeland_personalization_appearance_context_v1_round_corner_radius(
        int32_t radius) override;
    void treeland_personalization_appearance_context_v1_icon_theme(
        const QString &theme_name) override;
    void treeland_personalization_appearance_context_v1_active_color(
        const QString &active_color) override;
    void treeland_personalization_appearance_context_v1_window_opacity(uint32_t opacity) override;
    void treeland_personalization_appearance_context_v1_window_theme_type(uint32_t type) override;
    void treeland_personalization_appearance_context_v1_window_titlebar_height(
        uint32_t height) override;
};

QT_END_NAMESPACE

#endif // CUSTOMEXTENSION_H
