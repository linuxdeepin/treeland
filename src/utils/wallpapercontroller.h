// Copyright (C) 2024 Dingyuan Zhang <zhangdingyuan@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <woutput.h>

#include <QQmlEngine>
#include <QQuickItem>

class WallpaperController : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(Waylib::Server::WOutput* output READ output WRITE setOutput NOTIFY outputChanged)
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(AnimationType animationType READ animationType WRITE setAnimationType NOTIFY animationTypeChanged)

    QML_ELEMENT

public:
    explicit WallpaperController(QQuickItem *parent = nullptr);
    ~WallpaperController();

    Q_INVOKABLE WallpaperController *get(Waylib::Server::WOutput *output);

    enum AnimationType {
        Normal,
        Scale,
    };
    Q_ENUM(AnimationType)

Q_SIGNALS:
    void sourceChanged();
    void animationTypeChanged();
    void outputChanged();

public:
    void setSource(const QString &source);

    inline QString source() const { return m_source; }

    void setAnimationType(AnimationType type);

    inline AnimationType animationType() const { return m_animationType; }

    void setOutput(Waylib::Server::WOutput *output);

    inline Waylib::Server::WOutput *output() const { return m_output; }

private:
    Waylib::Server::WOutput *m_output;
    QString m_source;
    AnimationType m_animationType{ Normal };
};
