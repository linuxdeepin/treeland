// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "deepintheme.h"

#include "seat/helper.h"
#include "seatuserconfig.hpp"
#include "treelanduserconfig.hpp"

#include <DGuiApplicationHelper>

#include <QGuiApplication>
#include <QSizeF>
#include <QStyleHints>
#include <private/qguiapplication_p.h>

DCORE_USE_NAMESPACE;
DGUI_USE_NAMESPACE;

QDeepinTheme::QDeepinTheme() = default;

QDeepinTheme::~QDeepinTheme()
{
    disconnectConfig();
    disconnectSeatConfig();
}

const QPalette *QDeepinTheme::palette(QPlatformTheme::Palette type) const
{
    if (type != QPlatformTheme::SystemPalette) {
        return QGenericUnixTheme::palette(type);
    }
    static QPalette palette;
    palette = DGuiApplicationHelper::instance()->applicationPalette();
    return &palette;
}

QVariant QDeepinTheme::themeHint(ThemeHint hint) const
{
    if (!m_config)
        return QGenericUnixTheme::themeHint(hint);

    switch (hint) {
    case CursorFlashTime:
        return m_config->cursorBlink() ? m_config->cursorBlinkTime() : 0;
    case MouseDoubleClickInterval:
        return m_config->doubleClickTime();
    case MouseDoubleClickDistance:
        return m_config->doubleClickDistance();
    case StartDragDistance:
        return m_config->dndDragThreshold();
    case MouseCursorTheme:
        return m_config->cursorThemeName();
    case MouseCursorSize:
        return QSizeF(m_config->cursorSize(), m_config->cursorSize());
    case KeyboardAutoRepeatRate:
        return m_seatConfig ? m_seatConfig->keyboardRate() : QGenericUnixTheme::themeHint(hint);
    case KeyboardInputInterval:
        return m_seatConfig ? m_seatConfig->keyboardDelay() : QGenericUnixTheme::themeHint(hint);
    default:
        break;
    }
    return QGenericUnixTheme::themeHint(hint);
}

const QFont *QDeepinTheme::font(Font type) const
{
    if (!m_config)
        return QGenericUnixTheme::font(type);

    switch (type) {
    case SystemFont: {
        static QFont f;
        f.setFamily(m_config->font());
        f.setPointSize(m_config->fontSize() / 10.0);
        return &f;
    }
    case FixedFont: {
        static QFont f;
        f.setFamily(m_config->monoFont());
        f.setPointSize(m_config->fontSize() / 10.0);
        return &f;
    }
    default:
        break;
    }
    return QGenericUnixTheme::font(type);
}

void QDeepinTheme::bindConfig(TreelandUserConfig *config)
{
    if (m_config == config)
        return;

    disconnectConfig();
    m_config = config;
    if (!m_config)
        return;

    auto addConnection = [this](const QMetaObject::Connection &conn) {
        m_connections.push_back(conn);
    };

    addConnection(QObject::connect(m_config, &TreelandUserConfig::cursorBlinkChanged, m_config, [this]() { applyCursorSettings(); }));
    addConnection(QObject::connect(m_config, &TreelandUserConfig::cursorBlinkTimeChanged, m_config, [this]() { applyCursorSettings(); }));
    addConnection(QObject::connect(m_config, &TreelandUserConfig::doubleClickTimeChanged, m_config, [this]() { applyStyleHintSettings(); }));
    addConnection(QObject::connect(m_config, &TreelandUserConfig::doubleClickDistanceChanged, m_config, [this]() { applyThemeSettings(); }));
    addConnection(QObject::connect(m_config, &TreelandUserConfig::dndDragThresholdChanged, m_config, [this]() { applyStyleHintSettings(); }));
    addConnection(QObject::connect(m_config, &TreelandUserConfig::fontChanged, m_config, [this]() { applyFontSettings(); }));
    addConnection(QObject::connect(m_config, &TreelandUserConfig::monoFontChanged, m_config, [this]() { applyFontSettings(); }));
    addConnection(QObject::connect(m_config, &TreelandUserConfig::fontSizeChanged, m_config, [this]() { applyFontSettings(); }));
    addConnection(QObject::connect(m_config, &TreelandUserConfig::iconThemeNameChanged, m_config, [this]() { applyThemeSettings(); }));
    addConnection(QObject::connect(m_config, &TreelandUserConfig::themeNameChanged, m_config, [this]() { applyThemeSettings(); }));
    addConnection(QObject::connect(m_config, &TreelandUserConfig::preferDarkChanged, m_config, [this]() { applyStyleHintSettings(); }));
    addConnection(QObject::connect(m_config, &TreelandUserConfig::cursorThemeNameChanged, m_config, [this]() { applyThemeSettings(); }));
    addConnection(QObject::connect(m_config, &TreelandUserConfig::cursorSizeChanged, m_config, [this]() { applyThemeSettings(); }));

    applyAllSettings();
}

void QDeepinTheme::applyAllSettings()
{
    applyCursorSettings();
    applyStyleHintSettings();
    applyFontSettings();
    applyThemeSettings();
    applyKeyboardSettings();
}

void QDeepinTheme::applyCursorSettings()
{
    if (!m_config)
        return;

    int flashTime = m_config->cursorBlink() ? m_config->cursorBlinkTime() : 0;
    QGuiApplication::styleHints()->setCursorFlashTime(flashTime);
}

void QDeepinTheme::applyFontSettings()
{
    if (!m_config)
        return;

    QFont systemFont;
    systemFont.setFamily(m_config->font());
    systemFont.setPointSize(m_config->fontSize() / 10.0);
    QGuiApplication::setFont(systemFont);
}

void QDeepinTheme::applyThemeSettings()
{
    if (!m_config)
        return;

    QGuiApplicationPrivate::handleThemeChanged();
}

void QDeepinTheme::applyStyleHintSettings()
{
    if (!m_config)
        return;

    auto *hints = QGuiApplication::styleHints();
    hints->setMouseDoubleClickInterval(m_config->doubleClickTime());
    hints->setStartDragDistance(m_config->dndDragThreshold());

    if (m_config->preferDark()) {
        hints->setColorScheme(Qt::ColorScheme::Dark);
    } else {
        hints->setColorScheme(Qt::ColorScheme::Light);
    }
}

void QDeepinTheme::disconnectConfig()
{
    for (const auto &conn : std::as_const(m_connections)) {
        QObject::disconnect(conn);
    }
    m_connections.clear();
    m_config = nullptr;
}

void QDeepinTheme::bindSeatConfig(SeatUserDConfig *config)
{
    if (m_seatConfig == config)
        return;

    disconnectSeatConfig();
    m_seatConfig = config;
    if (!m_seatConfig)
        return;

    auto addConnection = [this](const QMetaObject::Connection &conn) {
        m_seatConnections.push_back(conn);
    };

    addConnection(QObject::connect(m_seatConfig, &SeatUserDConfig::keyboardRateChanged,
                                   m_seatConfig, [this]() { applyKeyboardSettings(); }));
    addConnection(QObject::connect(m_seatConfig, &SeatUserDConfig::keyboardDelayChanged,
                                   m_seatConfig, [this]() { applyKeyboardSettings(); }));

    applyKeyboardSettings();
}

void QDeepinTheme::applyKeyboardSettings()
{
    if (!m_seatConfig)
        return;

    auto *hints = QGuiApplication::styleHints();
    hints->setKeyboardAutoRepeatRate(m_seatConfig->keyboardRate());
    hints->setKeyboardInputInterval(m_seatConfig->keyboardDelay());
}

void QDeepinTheme::disconnectSeatConfig()
{
    for (const auto &conn : std::as_const(m_seatConnections)) {
        QObject::disconnect(conn);
    }
    m_seatConnections.clear();
    m_seatConfig = nullptr;
}
