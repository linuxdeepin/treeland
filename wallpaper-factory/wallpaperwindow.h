// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QObject>
#include <QWindow>

class WallpaperWindowPrivate;

class WallpaperWindow : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
public:
    ~WallpaperWindow() override;

    QString source();
    void setSource(const QString &source);

    bool eventFilter(QObject *watched, QEvent *event) override;

    static WallpaperWindow *get(QWindow *window);
    static WallpaperWindow *qmlAttachedProperties(QObject *object);

Q_SIGNALS:
    void sourceChanged();

private:
    void initializeShellIntegration();

    WallpaperWindow(QWindow *window);

private:
    std::unique_ptr<WallpaperWindowPrivate> d;
};
