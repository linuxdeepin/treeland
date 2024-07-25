// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "helper.h"

#include <woutput.h>

#include <QQmlEngine>
#include <QQuickItem>

class WallpaperProxy : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(Waylib::Server::WOutput* output READ output WRITE setOutput NOTIFY outputChanged)
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(Helper::WallpaperType type READ type WRITE setType NOTIFY typeChanged)

    QML_ELEMENT

public:
    explicit WallpaperProxy(QQuickItem *parent = nullptr);

    WallpaperProxy *get(Waylib::Server::WOutput *output);

Q_SIGNALS:
    void sourceChanged();
    void typeChanged();
    void outputChanged();

public:
    void setSource(const QString &source);

    inline QString source() const { return m_source; }

    Helper::WallpaperType type() const { return m_type; }

    void setOutput(Waylib::Server::WOutput *output);

    inline Waylib::Server::WOutput *output() const { return m_output; }

private:
    friend class WallpaperController;
    void setType(Helper::WallpaperType type);

private:
    Waylib::Server::WOutput *m_output;
    QString m_source;
    Helper::WallpaperType m_type{ Helper::WallpaperType::Normal };
};
