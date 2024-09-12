// Copyright (C) 2024 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"
#include <woutput.h>
#include <private/qquickimage_p.h>

WAYLIB_SERVER_USE_NAMESPACE

class WallpaperImageProvider;
class WWallpaperImage : public QQuickImage
{
    Q_OBJECT
    Q_PROPERTY(int userId READ userId WRITE setUserId NOTIFY userIdChanged FINAL)
    Q_PROPERTY(int workspace READ workspace WRITE setWorkspace NOTIFY workspaceChanged FINAL)
    Q_PROPERTY(WOutput* output READ output WRITE setOutput NOTIFY outputChanged FINAL)

    QML_NAMED_ELEMENT(WallpaperImage)
    QML_ADDED_IN_VERSION(1, 0)

public:
    WWallpaperImage(QQuickItem *parent = nullptr);
    ~WWallpaperImage();

    int userId();
    void setUserId(const int id);

    int workspace();
    void setWorkspace(const int id);

    WOutput* output();
    void setOutput(WOutput* output);

    Q_INVOKABLE void registerProvider();

Q_SIGNALS:
    void userIdChanged();
    void outputChanged();
    void workspaceChanged();

protected:
    void updateSource();
    void updateWallpaperTexture(const QString& id, int size);

private:
    int m_userId = -1;
    int m_workspaceId = -1;
    WOutput* m_output = nullptr;
};
