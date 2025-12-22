// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "modules/personalization/impl/types.h"

#include <QObject>

#include <functional>

struct treeland_personalization_manager_v1;

struct personalization_appearance_context_v1 : public QObject
{

    Q_OBJECT
public:
    personalization_appearance_context_v1(struct wl_client *client,
                                          struct wl_resource *manager_resource,
                                          uint32_t id);
    ~personalization_appearance_context_v1();

    static personalization_appearance_context_v1 *fromResource(struct wl_resource *resource);

    void setRoundCornerRadius(int32_t radius);
    void sendRoundCornerRadius(int32_t radius);

    void setIconTheme(const char *theme_name);
    void sendIconTheme(const char *icon_theme);

    void setActiveColor(const char *color);
    void sendActiveColor(const char *color);

    void setWindowOpacity(uint32_t opacity);
    void sendWindowOpacity(uint32_t opacity);

    void setWindowThemeType(uint32_t type);
    void sendWindowThemeType(uint32_t type);

    void setWindowTitlebarHeight(uint32_t height);
    void sendWindowTitlebarHeight(uint32_t height);

Q_SIGNALS:
    void beforeDestroy();
    void roundCornerRadiusChanged(int32_t radius);
    void iconThemeChanged(const QString &iconTheme);
    void activeColorChanged(const QString &color);
    void windowOpacityChanged(uint32_t opacity);
    void windowThemeTypeChanged(uint32_t type);
    void titlebarHeightChanged(uint32_t height);

    void requestRoundCornerRadius();
    void requestIconTheme();
    void requestActiveColor();
    void requestWindowOpacity();
    void requestWindowThemeType();
    void requestWindowTitlebarHeight();

private:
    treeland_personalization_manager_v1 *m_manager{ nullptr };
    struct wl_resource *m_resource{ nullptr };
};
