// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "settingmanager.h"
#include "common/treelandlogging.h"

const static qreal BASE_DPI = 96;
const static qreal XSETTINGS_BASE_DPI_FIXED = BASE_DPI * 1024;

SettingManager::SettingManager(xcb_connection_t *connection, QObject *parent)
    : QObject(parent)
    , m_resource(new XResource(connection, this))
    , m_settings(new XSettings(connection, this))
{
}

SettingManager::~SettingManager()
{
}

void SettingManager::setGTKTheme(const QString &themeName)
{
    m_resource->setPropertyValue(XResource::toString(XResource::Gtk_ThemeName).toLatin1(), themeName);
    m_settings->setPropertyValue(XSettings::toString(XSettings::Gtk_ThemeName).toLatin1(), themeName);
}

QString SettingManager::GTKTheme() const
{
    return m_settings->getPropertyValue(XSettings::toString(XSettings::Gtk_ThemeName).toLatin1()).toString();
}

void SettingManager::setFont(const QString &name)
{
    m_resource->setPropertyValue(XResource::toString(XResource::Gtk_FontName).toLatin1(), name);
    m_settings->setPropertyValue(XSettings::toString(XSettings::Gtk_FontName).toLatin1(), name);
}

QString SettingManager::font() const
{
    return m_settings->getPropertyValue(XSettings::toString(XSettings::Gtk_FontName).toLatin1()).toString();
}

void SettingManager::setIconTheme(const QString &theme)
{
    m_resource->setPropertyValue(XResource::toString(XResource::Gtk_IconThemeName).toLatin1(), theme);
    m_settings->setPropertyValue(XSettings::toString(XSettings::Gtk_IconThemeName).toLatin1(), theme);
}

QString SettingManager::iconTheme() const
{
    return m_settings->getPropertyValue(XSettings::toString(XSettings::Gtk_IconThemeName).toLatin1()).toString();
}

void SettingManager::setSoundTheme(const QString &theme)
{
    m_resource->setPropertyValue(XResource::toString(XResource::Net_SoundThemeName).toLatin1(), theme);
    m_settings->setPropertyValue(XSettings::toString(XSettings::Net_SoundThemeName).toLatin1(), theme);
}

QString SettingManager::soundTheme() const
{
    return m_settings->getPropertyValue(XSettings::toString(XSettings::Net_SoundThemeName).toLatin1()).toString();
}

void SettingManager::setCursorTheme(const QString &theme)
{
    m_resource->setPropertyValue(XResource::toString(XResource::Gtk_CursorThemeName).toLatin1(), theme);
    m_settings->setPropertyValue(XSettings::toString(XSettings::Gtk_CursorThemeName).toLatin1(), theme);
}

QString SettingManager::cursorTheme() const
{
    return m_settings->getPropertyValue(XSettings::toString(XSettings::Gtk_CursorThemeName).toLatin1()).toString();
}

void SettingManager::setCursorSize(qreal value)
{
    m_resource->setPropertyValue(XResource::toString(XResource::Xcursor_Size).toLatin1(), value);
    m_settings->setPropertyValue(XSettings::toString(XSettings::Xcursor_Size).toLatin1(), value);
}

qreal SettingManager::cursorSize() const
{
    return m_settings->getPropertyValue(XSettings::toString(XSettings::Gtk_CursorThemeName).toLatin1()).toReal();
}

void SettingManager::setDoubleClickInterval(int interval)
{
    m_settings->setPropertyValue(XSettings::toString(XSettings::Net_DoubleClickTime).toLatin1(), interval);
}

void SettingManager::setGlobalScale(qreal scale)
{
    m_resource->setPropertyValue(XResource::toString(XResource::Xft_DPI).toLatin1(), scale * BASE_DPI);
    m_settings->setPropertyValue(XSettings::toString(XSettings::Gdk_WindowScalingFactor).toLatin1(), qFloor(scale));
    m_settings->setPropertyValue(XSettings::toString(XSettings::Gdk_UnscaledDPI).toLatin1(), scale * XSETTINGS_BASE_DPI_FIXED);
    m_settings->setPropertyValue(XSettings::toString(XSettings::Xft_DPI).toLatin1(), scale * XSETTINGS_BASE_DPI_FIXED);
}

qreal SettingManager::globalScale() const
{
    return m_settings->getPropertyValue(XSettings::toString(XSettings::Gdk_UnscaledDPI).toLatin1()).toReal() / XSETTINGS_BASE_DPI_FIXED;
}

void SettingManager::apply()
{
    m_resource->apply();
    m_settings->apply();
}
