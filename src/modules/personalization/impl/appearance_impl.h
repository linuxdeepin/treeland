// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include "types.h"

class treeland_personalization_manager_v1;

class personalization_appearance_context_v1 : public QObject
{

    Q_OBJECT
public:
    personalization_appearance_context_v1(struct wl_client *client,
                                          struct wl_resource *manager_resource,
                                          uint32_t id);
    ~personalization_appearance_context_v1();

    static personalization_appearance_context_v1 *fromResource(struct wl_resource *resource);

    void setRoundCornerRadius(int32_t radius);
    void setIconTheme(const char *theme_name);

    void sendRoundCornerRadius() const;
    void sendIconTheme() const;

    void setActiveColor(const char *color);
    void sendActiveColor() const;

    void setWindowOpacity(uint32_t opacity);
    void sendWindowOpacity() const;

    void setWindowThemeType(uint32_t type);
    void sendWindowThemeType() const;

	void setWindowTitlebarHeight(uint32_t height);
	void sendWindowTitlebarHeight() const;

    inline int32_t roundCornerRadius() const
    {
        return m_radius;
    }

    inline QString iconTheme() const
    {
        return m_iconTheme;
    }

    inline QString activeColor() const
    {
        return m_activeColor;
    }

    inline uint32_t windowOpacity() const
    {
        return m_windowOpacity;
    }

    inline ThemeType windowThemeType() const
    {
        return m_windowThemeType;
    }

    inline uint32_t titlebarHeight() const {
        return m_titlebarHeight;
    }

Q_SIGNALS:
    void roundCornerRadiusChanged();
    void iconThemeChanged();
    void activeColorChanged();
    void windowOpacityChanged();
    void windowThemeTypeChanged();
    void titlebarHeightChanged();

private:
    void sendState(std::function<void(struct wl_resource *)> func);

private:
    treeland_personalization_manager_v1 *m_manager{ nullptr };
    struct wl_resource *m_resource{ nullptr };

    // TODO: move to settings
    int32_t m_radius{ 0 };
    QString m_iconTheme;
    QString m_activeColor;
    uint32_t m_windowOpacity{ 0 };
    ThemeType m_windowThemeType{ ThemeType::Auto };
    uint32_t m_titlebarHeight { 0 };
};
