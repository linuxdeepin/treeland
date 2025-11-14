// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "xresource.h"
#include "xsettings.h"

#include <QObject>

class SettingManager : public QObject
{
    Q_OBJECT
public:
    explicit SettingManager(xcb_connection_t *connection, QObject *parent = nullptr);
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

private:
    XResource *m_resource = nullptr;
    XSettings *m_settings = nullptr;
};
