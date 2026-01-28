// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "settingmanager.h"

#include <QMetaObject>

const static qreal BASE_DPI = 96;
const static qreal XSETTINGS_BASE_DPI_FIXED = BASE_DPI * 1024;

SettingManager::SettingManager(xcb_connection_t *connection, QObject *parent, bool async)
    : QObject(parent)
    , m_async(async)
    , m_resource(new XResource(connection, this))
    , m_settings(new XSettings(connection, this))
{
    if (m_async) {
        m_thread = new QThread;
        this->moveToThread(m_thread);
        m_thread->start();
    }
}

SettingManager::~SettingManager()
{
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
    }
}

void SettingManager::setGTKThemeAsync(const QString &themeName)
{
    QMetaObject::invokeMethod(this,
                              "onSetGTKTheme",
                              Qt::QueuedConnection,
                              Q_ARG(QString, themeName));
}

QString SettingManager::GTKThemeAsync() const
{
    QString result;
    QMetaObject::invokeMethod(
        const_cast<SettingManager *>(this),
        "onGetGTKTheme",
        Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(QString, result)
        );

    return result;
}

void SettingManager::setFontAsync(const QString &name)
{
    QMetaObject::invokeMethod(this,
                              "onSetFont",
                              Qt::QueuedConnection,
                              Q_ARG(QString, name));
}

QString SettingManager::fontAsync() const
{
    QString result;
    QMetaObject::invokeMethod(
        const_cast<SettingManager *>(this),
        "onGetFont",
        Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(QString, result)
        );

    return result;
}

void SettingManager::setIconThemeAsync(const QString &theme)
{
    QMetaObject::invokeMethod(this,
                              "onSetIconTheme",
                              Qt::QueuedConnection,
                              Q_ARG(QString, theme));
}

QString SettingManager::iconThemeAsync() const
{
    QString result;
    QMetaObject::invokeMethod(
        const_cast<SettingManager *>(this),
        "onGetIconTheme",
        Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(QString, result)
        );

    return result;
}

void SettingManager::setSoundThemeAsync(const QString &theme)
{
    QMetaObject::invokeMethod(this,
                              "onSetSoundTheme",
                              Qt::QueuedConnection,
                              Q_ARG(QString, theme));
}

QString SettingManager::soundThemeAsync() const
{
    QString result;
    QMetaObject::invokeMethod(
        const_cast<SettingManager *>(this),
        "onGetSoundTheme",
        Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(QString, result)
        );

    return result;
}

void SettingManager::setCursorThemeAsync(const QString &theme)
{
    QMetaObject::invokeMethod(this,
                              "onSetCursorTheme",
                              Qt::QueuedConnection,
                              Q_ARG(QString, theme));
}

QString SettingManager::cursorThemeAsync() const
{
    QString result;
    QMetaObject::invokeMethod(
        const_cast<SettingManager *>(this),
        "onGetCursorTheme",
        Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(QString, result)
        );

    return result;
}

void SettingManager::setCursorSizeAsync(qreal value)
{
    QMetaObject::invokeMethod(this,
                              "onSetCursorSize",
                              Qt::QueuedConnection,
                              Q_ARG(qreal, value));
}

qreal SettingManager::cursorSizeAsync() const
{
    qreal result = 0.0;
    QMetaObject::invokeMethod(
        const_cast<SettingManager *>(this),
        "onGetcursorSize",
        Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(qreal, result)
        );

    return result;
}

void SettingManager::setDoubleClickIntervalAsync(int interval)
{
    QMetaObject::invokeMethod(this,
                              "onSetDoubleClickInterval",
                              Qt::QueuedConnection,
                              Q_ARG(int, interval));
}

void SettingManager::setGlobalScaleAsync(qreal scale)
{
    QMetaObject::invokeMethod(this,
                              "onSetGlobalScale",
                              Qt::QueuedConnection,
                              Q_ARG(int, scale));
}

qreal SettingManager::globalScaleAsync() const
{
    qreal result = 0.0;
    QMetaObject::invokeMethod(
        const_cast<SettingManager *>(this),
        "onGlobalScale",
        Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(qreal, result)
        );

    return result;
}

void SettingManager::applyAsync()
{
    QMetaObject::invokeMethod(this,
                              "onApply",
                              Qt::QueuedConnection);
}

void SettingManager::onSetGTKTheme(const QString &themeName)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Gtk_ThemeName),
                                 themeName);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gtk_ThemeName),
                                 themeName);
}

QString SettingManager::onGetGTKTheme() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gtk_ThemeName)).toString();
}

void SettingManager::onSetFont(const QString &name)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Gtk_FontName),
                                 name);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gtk_FontName),
                                 name);
}

QString SettingManager::onGetFont() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gtk_FontName)).toString();
}

void SettingManager::onSetIconTheme(const QString &theme)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Gtk_IconThemeName),
                                 theme);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gtk_IconThemeName),
                                 theme);
}

QString SettingManager::onGetIconTheme() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gtk_IconThemeName)).toString();
}

void SettingManager::onSetSoundTheme(const QString &theme)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Net_SoundThemeName),
                                 theme);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Net_SoundThemeName),
                                 theme);
}

QString SettingManager::onGetSoundTheme() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Net_SoundThemeName)).toString();
}

void SettingManager::onSetCursorTheme(const QString &theme)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Gtk_CursorThemeName),
                                 theme);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gtk_CursorThemeName),
                                 theme);
}

QString SettingManager::onGetCursorTheme() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gtk_CursorThemeName)).toString();
}

void SettingManager::onSetCursorSize(qreal value)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Xcursor_Size),
                                 value);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Xcursor_Size),
                                 value);
}

qreal SettingManager::onGetcursorSize() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Xcursor_Size)).toReal();
}

void SettingManager::onSetDoubleClickInterval(int interval)
{
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Net_DoubleClickTime),
                                 interval);
}

void SettingManager::onSetGlobalScale(qreal scale)
{
    m_resource->setPropertyValue(XResource::toByteArray(XResource::Xft_DPI),
                                 scale * BASE_DPI);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gdk_WindowScalingFactor),
                                 qFloor(scale));
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Gdk_UnscaledDPI),
                                 scale * XSETTINGS_BASE_DPI_FIXED);
    m_settings->setPropertyValue(XSettings::toByteArray(XSettings::Xft_DPI),
                                 scale * XSETTINGS_BASE_DPI_FIXED);
}

qreal SettingManager::onGetGlobalScale() const
{
    return m_settings->getPropertyValue(XSettings::toByteArray(XSettings::Gdk_UnscaledDPI)).toReal() / XSETTINGS_BASE_DPI_FIXED;
}

void SettingManager::onApply()
{
    m_resource->apply();
    m_settings->apply();
}

void SettingManager::setGTKTheme(const QString &themeName)
{
    if (async()) {
        setGTKThemeAsync(themeName);
    }

    onSetGTKTheme(themeName);
}

QString SettingManager::GTKTheme() const
{
    if (async()) {
        return GTKThemeAsync();
    }

    return onGetGTKTheme();
}

void SettingManager::setFont(const QString &name)
{
    if (async()) {
        setFontAsync(name);
    }

    onSetFont(name);
}

QString SettingManager::font() const
{
    if (async()) {
        return fontAsync();
    }

    return onGetFont();
}

void SettingManager::setIconTheme(const QString &theme)
{
    if (async()) {
        setIconThemeAsync(theme);
        return;
    }

    onSetIconTheme(theme);
}

QString SettingManager::iconTheme() const
{
    if (async()) {
        return iconThemeAsync();
    }

    return onGetIconTheme();
}

void SettingManager::setSoundTheme(const QString &theme)
{
    if (async()) {
        setSoundThemeAsync(theme);
        return;
    }

    onSetSoundTheme(theme);
}

QString SettingManager::soundTheme() const
{
    if (async()) {
        return soundThemeAsync();
    }

    return onGetSoundTheme();
}

void SettingManager::setCursorTheme(const QString &theme)
{
    if (async()) {
        setCursorThemeAsync(theme);
        return;
    }

    onSetCursorTheme(theme);
}

QString SettingManager::cursorTheme() const
{
    if (async()) {
        return cursorThemeAsync();
    }

    return onGetCursorTheme();
}

void SettingManager::setCursorSize(qreal value)
{
    if (async()) {
        setCursorSizeAsync(value);
        return;
    }

    onSetCursorSize(value);
}

qreal SettingManager::cursorSize() const
{
    if (async()) {
        return cursorSizeAsync();
    }

    return onGetcursorSize();
}

void SettingManager::setDoubleClickInterval(int interval)
{
    if (async()) {
        setDoubleClickIntervalAsync(interval);
        return;
    }

    onSetDoubleClickInterval(interval);
}

void SettingManager::setGlobalScale(qreal scale)
{
    if (async()) {
        setGlobalScaleAsync(scale);
        return;
    }

    onSetGlobalScale(scale);
}

qreal SettingManager::globalScale() const
{
    if (async()) {
        return globalScaleAsync();
    }

    return onGetGlobalScale();
}

void SettingManager::apply()
{
    if (async()) {
        applyAsync();
        return;
    }

    onApply();
}

bool SettingManager::async() const
{
    return m_async;
}
