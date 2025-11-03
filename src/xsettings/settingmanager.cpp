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
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Gtk_ThemeName), themeName);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gtk_ThemeName), themeName);
}

QString SettingManager::GTKTheme() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gtk_ThemeName)).toString();
}

void SettingManager::setFont(const QString &name)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Gtk_FontName), name);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gtk_FontName), name);
}

QString SettingManager::font() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gtk_FontName)).toString();
}

void SettingManager::setIconTheme(const QString &theme)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Gtk_IconThemeName), theme);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gtk_IconThemeName), theme);
}

QString SettingManager::iconTheme() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gtk_IconThemeName)).toString();
}

void SettingManager::setSoundTheme(const QString &theme)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Net_SoundThemeName), theme);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Net_SoundThemeName), theme);
}

QString SettingManager::soundTheme() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Net_SoundThemeName)).toString();
}

void SettingManager::setCursorTheme(const QString &theme)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Gtk_CursorThemeName), theme);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gtk_CursorThemeName), theme);
}

QString SettingManager::cursorTheme() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gtk_CursorThemeName)).toString();
}

void SettingManager::setCursorSize(qreal value)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Xcursor_Size), value);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Xcursor_Size), value);
}

qreal SettingManager::cursorSize() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Xcursor_Size)).toReal();
}

void SettingManager::setDoubleClickInterval(int interval)
{
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Net_DoubleClickTime), interval);
}

void SettingManager::setGlobalScale(qreal scale)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Xft_DPI), scale * BASE_DPI);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gdk_WindowScalingFactor), qFloor(scale));
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gdk_UnscaledDPI), scale * XSETTINGS_BASE_DPI_FIXED);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Xft_DPI), scale * XSETTINGS_BASE_DPI_FIXED);
}

qreal SettingManager::globalScale() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gdk_UnscaledDPI)).toReal() / XSETTINGS_BASE_DPI_FIXED;
}

void SettingManager::apply()
{
    m_resource->apply();
    m_settings->apply();
}
