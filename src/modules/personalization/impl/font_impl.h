// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "treeland-personalization-manager-protocol.h"

#include <wayland-server-core.h>

#include <qwdisplay.h>

#include <QObject>
#include <QStringList>

class treeland_personalization_manager_v1;

struct personalization_font_context_v1 : public QObject
{
    Q_OBJECT
public:
    personalization_font_context_v1(struct wl_client *client,
                                    struct wl_resource *manager_resource,
                                    uint32_t id);
    ~personalization_font_context_v1();

    static personalization_font_context_v1 *fromResource(struct wl_resource *resource);

    void setFont(const char *font_name);
    void setMonospaceFont(const char *font_name);
    void setFontSize(uint32_t size);

    void sendFont() const;
    void sendMonospaceFont() const;
    void sendFontSize() const;

    inline QString font() const
    {
        return m_fontName;
    }

    inline QString monoFont() const
    {
        return m_monoFontName;
    }

    inline uint32_t fontSize() const
    {
        return m_fontSize;
    }

Q_SIGNALS:
    void before_destroy();
    void fontChanged();
    void monoFontChanged();
    void fontSizeChanged();

private:
    void sendState(std::function<void(struct wl_resource *)> func);

private:
    QString m_fontName;
    QString m_monoFontName;
    uint32_t m_fontSize{ 0 };

    treeland_personalization_manager_v1 *m_manager;
    wl_resource *m_resource;
};
