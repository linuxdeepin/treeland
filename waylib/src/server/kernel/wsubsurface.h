// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"

#include <QObject>
#include <QPointF>
#include <QPointer>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WSurface;
class WSurfacePrivate;
class WSubsurfacePrivate;
class RemoteSubsurfaceContext;

class WAYLIB_SERVER_EXPORT WSubsurface : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WSubsurface)
    Q_PROPERTY(Type type READ type CONSTANT FINAL)
    Q_PROPERTY(Place place READ place NOTIFY placeChanged FINAL)
    Q_PROPERTY(QPointF position READ position NOTIFY positionChanged FINAL)
    Q_PROPERTY(bool mapped READ isMapped NOTIFY mappedChanged FINAL)
    Q_PROPERTY(WSurface *parentSurface READ parentSurface CONSTANT FINAL)
    Q_PROPERTY(WSurface *surface READ surface CONSTANT FINAL)

public:
    enum class Type {
        Standard,
        Remote,
    };
    Q_ENUM(Type)

    enum class Place {
        Below,
        Above,
    };
    Q_ENUM(Place)

    ~WSubsurface() override;

    Type type() const;
    Place place() const;
    QPointF position() const;
    bool isMapped() const;

    WSurface *parentSurface() const;
    WSurface *surface() const;

Q_SIGNALS:
    void positionChanged(const QPointF &position);
    void placeChanged(Place place);
    void mappedChanged(bool mapped);

private:
    WSubsurface(Type type,
                WSurface *parentSurface,
                WSurface *surface);

    void setPlace(Place place);
    void setPosition(const QPointF &position);
    void setMapped(bool mapped);

    friend class WSurfacePrivate;
    friend class RemoteSubsurfaceContext;
};

WAYLIB_SERVER_END_NAMESPACE
