// Copyright (C) 2023 Dingyuan Zhang <lxz@mkacg.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>

class treeland_personalization_manager_v1;

class personalization_appearance_context_v1 : public QObject
{

    Q_OBJECT
public:
    personalization_appearance_context_v1(struct wl_client *client,
                                          struct wl_resource *manager_resource,
                                          uint32_t id);

    static personalization_appearance_context_v1 *fromResource(struct wl_resource *resource);

    void setRoundCornerRadius(int32_t radius);
    void setFont(const char *font_name);
    void setMonospaceFont(const char *font_name);
    void setCursorTheme(const char *theme_name);
    void setIconTheme(const char *theme_name);

    void sendRoundCornerRadius() const;
    void sendFont() const;
    void sendMonospaceFont() const;
    void sendCursorTheme() const;
    void sendIconTheme() const;

    inline int32_t roundCornerRadius() const
    {
        return m_radius;
    }

    inline QString font() const
    {
        return m_fontName;
    }

    inline QString monoFont() const
    {
        return m_monoFontName;
    }

    inline QString cursorTheme() const
    {
        return m_cursorTheme;
    }

    inline QString iconTheme() const
    {
        return m_iconTheme;
    }

Q_SIGNALS:
    void roundCornerRadiusChanged();
    void fontChanged();
    void monoFontChanged();
    void cursorThemeChanged();
    void iconThemeChanged();

private:
    void sendState(std::function<void(struct wl_resource *)> func);

private:
    treeland_personalization_manager_v1 *m_manager{ nullptr };
    struct wl_resource *m_resource{ nullptr };

    // TODO: move to settings
    int32_t m_radius{ 0 };
    QString m_fontName;
    QString m_monoFontName;
    QString m_cursorTheme;
    QString m_iconTheme;
};
