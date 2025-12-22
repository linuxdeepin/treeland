// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "treeland-personalization-manager-protocol.h"

#include <wayland-server-core.h>

#include <qwdisplay.h>

#include <QObject>
#include <QStringList>

struct treeland_personalization_manager_v1;

struct personalization_font_context_v1 : public QObject
{
    Q_OBJECT
public:
    personalization_font_context_v1(struct wl_client *client,
                                    struct wl_resource *manager_resource,
                                    uint32_t id);

    static personalization_font_context_v1 *fromResource(struct wl_resource *resource);

    void sendFont(const QString &font) const;
    void sendMonospaceFont(const QString &font) const;
    void sendFontSize(uint32_t size) const;

Q_SIGNALS:
    void beforeDestroy();
    void fontChanged(const QString &font);
    void monoFontChanged(const QString &font);
    void fontSizeChanged(uint32_t size);
    void requestFont();
    void requestMonoFont();
    void requestFontSize();

private:
    treeland_personalization_manager_v1 *m_manager;
    wl_resource *m_resource;
};
