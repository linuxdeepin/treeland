// Copyright (C) 2023 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "personalization-server-protocol.h"
#include "personalization_manager_impl.h"
#include "treelandhelper.h"

#include <qtmetamacros.h>
#include <wxdgsurface.h>
#include <wquickwaylandserver.h>

#include <QObject>
#include <QQmlEngine>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WXdgSurface;
class WSurface;
WAYLIB_SERVER_END_NAMESPACE

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class QuickPersonalizationManager;
class WAYLIB_SERVER_EXPORT QuickPersonalizationManagerAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool backgroundWallpaper READ backgroundWallpaper NOTIFY backgroundWallpaperChanged FINAL)
    QML_ANONYMOUS

public:
    enum BackGroundType {
        Normal,
        Wallpaper,
        Blend
    };
    Q_ENUM(BackGroundType)

    QuickPersonalizationManagerAttached(WSurface *target, QuickPersonalizationManager *manager);

    bool backgroundWallpaper() const {
        return m_backgroundType == Wallpaper;
    };

Q_SIGNALS:
    void backgroundWallpaperChanged();

private:
    WSurface *m_target;
    QuickPersonalizationManager *m_manager;
    BackGroundType m_backgroundType = Normal;
};

class QuickPersonalizationManagerPrivate;
class PersonalizationWindowContext;
class QuickPersonalizationManager : public WQuickWaylandServerInterface, public WObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(PersonalizationManager)
    W_DECLARE_PRIVATE(QuickPersonalizationManager)
    QML_ATTACHED(QuickPersonalizationManagerAttached)

public:
    explicit QuickPersonalizationManager(QObject *parent = nullptr);

    void onWindowContextCreated(PersonalizationWindowContext *context);
    static QuickPersonalizationManagerAttached *qmlAttachedProperties(QObject *target);

    void onBackgroundTypeChanged(PersonalizationWindowContext *context);
Q_SIGNALS:
    void backgroundTypeChanged(WSurface *surface, uint32_t type);

private:
    void create() override;
};
