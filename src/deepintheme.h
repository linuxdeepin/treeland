// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QFont>
#include <QPalette>
#include <QPointer>

#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#  include <private/qgenericunixtheme_p.h>
#else
#  include <private/qgenericunixthemes_p.h>
#endif

#include <functional>
#include <vector>

class TreelandUserConfig;

class QDeepinTheme : public QGenericUnixTheme
{
public:
    QDeepinTheme();
    ~QDeepinTheme() override;

    const QPalette *palette(QPlatformTheme::Palette type) const override;
    QVariant themeHint(ThemeHint hint) const override;
    const QFont *font(Font type) const override;

    void bindConfig(TreelandUserConfig *config);

private:
    void applyAllSettings();
    void applyCursorSettings();
    void applyFontSettings();
    void applyThemeSettings();
    void applyStyleHintSettings();

    void disconnectConfig();

    QPointer<TreelandUserConfig> m_config;
    std::vector<QMetaObject::Connection> m_connections;
};
