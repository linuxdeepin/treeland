// Copyright (C) 2024 justforlxz <zhangdingyuan@deepin.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef CUSTOMEXTENSION_H
#define CUSTOMEXTENSION_H

#include "qwayland-treeland-personalization-manager-v1.h"
#include "treeland-personalization-manager-protocol.h"

#include <QtGui/QWindow>
#include <QtWaylandClient/QWaylandClientExtension>

QT_BEGIN_NAMESPACE

class PersonalizationManager : public QWaylandClientExtensionTemplate<PersonalizationManager>,
                               public QtWayland::treeland_personalization_manager_v1
{
    Q_OBJECT
public:
    explicit PersonalizationManager();
};

class Appearance : public QWaylandClientExtensionTemplate<Appearance>,
                   public QtWayland::treeland_personalization_appearance_context_v1
{
    Q_OBJECT
public:
    explicit Appearance(struct ::treeland_personalization_appearance_context_v1 *object);

Q_SIGNALS:
    void cursorThemeChanged(const QString &theme);

protected:
    void treeland_personalization_appearance_context_v1_round_corner_radius(
        int32_t radius) override;
    void treeland_personalization_appearance_context_v1_font(const QString &font_name) override;
    void treeland_personalization_appearance_context_v1_monospace_font(
        const QString &font_name) override;
    void treeland_personalization_appearance_context_v1_cursor_theme(
        const QString &theme_name) override;
    void treeland_personalization_appearance_context_v1_icon_theme(
        const QString &theme_name) override;
};

QT_END_NAMESPACE

#endif // CUSTOMEXTENSION_H
