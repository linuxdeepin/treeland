// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "xresource.h"
#include "xsettings.h"

#include <QObject>
#include <QThread>

class SettingManager : public QObject
{
    Q_OBJECT
public:
    explicit SettingManager(xcb_connection_t *connection, QObject *parent = nullptr, bool async = true);
    ~SettingManager() override;

    void setGTKTheme(const QString &themeName);
    QString GTKTheme() const;

    void setFont(const QString &name);
    QString font() const;

    void setIconTheme(const QString &theme);
    QString iconTheme() const;

    void setSoundTheme(const QString &theme);
    QString soundTheme() const;

    void setCursorTheme(const QString &theme);
    QString cursorTheme() const;

    void setCursorSize(qreal value);
    qreal cursorSize() const;

    void setDoubleClickInterval(int interval);

    void setGlobalScale(qreal scale);
    qreal globalScale() const;

    void apply();

    bool async() const;
private:
    void setGTKThemeAsync(const QString &themeName);
    QString GTKThemeAsync() const;

    void setFontAsync(const QString &name);
    QString fontAsync() const;

    void setIconThemeAsync(const QString &theme);
    QString iconThemeAsync() const;

    void setSoundThemeAsync(const QString &theme);
    QString soundThemeAsync() const;

    void setCursorThemeAsync(const QString &theme);
    QString cursorThemeAsync() const;

    void setCursorSizeAsync(qreal value);
    qreal cursorSizeAsync() const;

    void setDoubleClickIntervalAsync(int interval);

    void setGlobalScaleAsync(qreal scale);
    qreal globalScaleAsync() const;

    void applyAsync();

private Q_SLOTS:
    void onSetGTKTheme(const QString &themeName);
    QString onGetGTKTheme() const;

    void onSetFont(const QString &name);
    QString onGetFont() const;

    void onSetIconTheme(const QString &theme);
    QString onGetIconTheme() const;

    void onSetSoundTheme(const QString &theme);
    QString onGetSoundTheme() const;

    void onSetCursorTheme(const QString &theme);
    QString onGetCursorTheme() const;

    void onSetCursorSize(qreal value);
    qreal onGetcursorSize() const;

    void onSetDoubleClickInterval(int interval);

    void onSetGlobalScale(qreal scale);
    qreal onGetGlobalScale() const;

    void onApply();

private:
    bool m_async;
    QThread *m_thread = nullptr;
    XResource *m_resource = nullptr;
    XSettings *m_settings = nullptr;
};
