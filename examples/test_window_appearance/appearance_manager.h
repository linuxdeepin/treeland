// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef APPEARANCE_MANAGER_H
#define APPEARANCE_MANAGER_H

#include "qwayland-treeland-appearance-unstable-v1.h"
#include "qwayland-treeland-appearance-manager-unstable-v1.h"

#include <QtWaylandClient/QWaylandClientExtension>

QT_BEGIN_NAMESPACE

// Read-only observer for user-level appearance settings. Any client may bind
// this global; the compositor pushes the current value of every setting
// immediately after bind and broadcasts an event whenever a setting changes.
class AppearanceObserver
    : public QWaylandClientExtensionTemplate<AppearanceObserver>
    , public QtWayland::treeland_appearance_v1
{
    Q_OBJECT
public:
    explicit AppearanceObserver();

Q_SIGNALS:
    void cursorThemeChanged(const QString &name);
    void cursorSizeChanged(uint32_t width, uint32_t height);
    void fontChanged(const QString &font_name);
    void monospaceFontChanged(const QString &font_name);
    void fontSizeChanged(uint32_t size);
    void iconThemeChanged(const QString &theme_name);
    void accentColorChanged(uint32_t r, uint32_t g, uint32_t b);
    void windowOpacityChanged(wl_fixed_t opacity);
    void colorSchemeChanged(uint32_t scheme);
    void titlebarHeightChanged(uint32_t height);
    void cornerRadiusChanged(uint32_t radius);

protected:
    void treeland_appearance_v1_cursor_theme(const QString &name) override;
    void treeland_appearance_v1_cursor_size(uint32_t width, uint32_t height) override;
    void treeland_appearance_v1_font(const QString &font_name) override;
    void treeland_appearance_v1_monospace_font(const QString &font_name) override;
    void treeland_appearance_v1_font_size(uint32_t size) override;
    void treeland_appearance_v1_icon_theme(const QString &theme_name) override;
    void treeland_appearance_v1_accent_color(uint32_t r, uint32_t g, uint32_t b) override;
    void treeland_appearance_v1_window_opacity(wl_fixed_t opacity) override;
    void treeland_appearance_v1_color_scheme(uint32_t scheme) override;
    void treeland_appearance_v1_titlebar_height(uint32_t height) override;
    void treeland_appearance_v1_corner_radius(uint32_t radius) override;
};

// Privileged manager for modifying user-level appearance settings. Binding
// without compositor-level authorization may result in requests being
// silently ignored or the connection being terminated.
class AppearanceManager
    : public QWaylandClientExtensionTemplate<AppearanceManager>
    , public QtWayland::treeland_appearance_manager_v1
{
    Q_OBJECT
public:
    explicit AppearanceManager();
};

QT_END_NAMESPACE

#endif // APPEARANCE_MANAGER_H
