// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>
#include <wglobal.h>
#include <QQuickItem>

QT_BEGIN_NAMESPACE
class QSGTexture;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputViewport;
class WOutputLayerPrivate;
class WAYLIB_SERVER_EXPORT WOutputLayer : public QObject
{
    Q_DECLARE_PRIVATE(WOutputLayer)
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(bool keepLayer READ keepLayer WRITE setKeepLayer NOTIFY keepLayerChanged FINAL)
    Q_PROPERTY(bool force READ force WRITE setForce NOTIFY forceChanged FINAL)
    Q_PROPERTY(Flags flags READ flags WRITE setFlags NOTIFY flagsChanged FINAL)
    Q_PROPERTY(QList<WOutputViewport*> outputs READ outputs WRITE setOutputs NOTIFY outputsChanged FINAL)
    Q_PROPERTY(QList<WOutputViewport*> inOutputsByHardware READ inOutputsByHardware NOTIFY inOutputsByHardwareChanged FINAL)
    Q_PROPERTY(int z READ z WRITE setZ NOTIFY zChanged FINAL)
    Q_PROPERTY(QPointF cursorHotSpot READ cursorHotSpot WRITE setCursorHotSpot NOTIFY cursorHotSpotChanged FINAL)
    QML_NAMED_ELEMENT(OutputLayer)
    QML_UNCREATABLE("OutputLayer is only available via attached properties")
    QML_ATTACHED(WOutputLayer)

public:
    enum Flag {
        SizeSensitive = 1 << 0,
        DontClip = 1 << 1,
        PreserveColorContents = 1 << 2,
        NoAlpha = 1 << 3,
        Cursor = 1 << 4,
    };
    Q_ENUM(Flag)
    Q_DECLARE_FLAGS(Flags, Flag)

    explicit WOutputLayer(QQuickItem *parent);

    static WOutputLayer *qmlAttachedProperties(QObject *target);

    QQuickItem *parent() const;

    bool enabled() const;
    void setEnabled(bool newEnabled);

    Flags flags() const;
    void setFlags(const Flags &newFlags);

    const QList<WOutputViewport *> &outputs() const;
    void setOutputs(const QList<WOutputViewport*> &newOutputList);

    const QList<WOutputViewport *> &inOutputsByHardware() const;

    int z() const;
    void setZ(int newZ);

    bool keepLayer() const;
    void setKeepLayer(bool newKeepLayer);

    bool force() const;
    void setForce(bool newForce);

    QPointF cursorHotSpot() const;
    void setCursorHotSpot(QPointF newCursorHotSpot);

Q_SIGNALS:
    void enabledChanged();
    void flagsChanged();
    void outputsChanged();
    void inOutputsByHardwareChanged();
    void zChanged();
    void keepLayerChanged();
    void forceChanged();
    void cursorHotSpotChanged();

private:
    void setAccepted(bool accepted);
    bool isAccepted() const;
    bool setInHardware(WOutputViewport *output, bool isHardware);

    friend class OutputLayer;
};

WAYLIB_SERVER_END_NAMESPACE

Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WOutputLayer::Flags)
