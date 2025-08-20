// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <woutput.h>

#include <QQmlEngine>
#include <QQuickItem>

Q_MOC_INCLUDE("wallpaper/wallpaperimage.h")

class WallpaperImage;

class WallpaperController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WOutput* output READ output WRITE setOutput NOTIFY outputChanged REQUIRED)
    Q_PROPERTY(WallpaperType type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(WallpaperImage* proxy READ proxy NOTIFY proxyChanged)
    Q_PROPERTY(bool lock READ lock WRITE setLock NOTIFY lockChanged)

    QML_ELEMENT

public:
    explicit WallpaperController(QObject *parent = nullptr);
    ~WallpaperController() override;

    enum WallpaperType
    {
        Normal,
        Scale,
        ScaleWithoutAnimation,
    };
    Q_ENUM(WallpaperType)

Q_SIGNALS:
    void typeChanged();
    void outputChanged();
    void proxyChanged();
    void lockChanged();

public:
    void setType(WallpaperType type);

    WallpaperType type() const
    {
        return m_type;
    }

    void setOutput(WAYLIB_SERVER_NAMESPACE::WOutput *output);

    inline WAYLIB_SERVER_NAMESPACE::WOutput *output() const
    {
        return m_output;
    }

    WallpaperImage *proxy() const;

    void setLock(bool state);
    bool lock() const;

private:
    void updateState();

private:
    WAYLIB_SERVER_NAMESPACE::WOutput *m_output;
    WallpaperType m_type{ WallpaperType::Normal };
};
