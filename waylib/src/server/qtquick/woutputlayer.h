// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>
#include <wglobal.h>
#include <QQuickItem>

QW_BEGIN_NAMESPACE
class QWBuffer;
QW_END_NAMESPACE

QT_BEGIN_NAMESPACE
class QSGTexture;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutputViewport;
class WOutputLayerPrivate;
class WAYLIB_SERVER_EXPORT WOutputLayer : public QObject, public WObject
{
    W_DECLARE_PRIVATE(WOutputLayer)
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged FINAL)
    Q_PROPERTY(Flags flags READ flags WRITE setFlags NOTIFY flagsChanged FINAL)
    Q_PROPERTY(QList<WOutputViewport*> outputs READ outputs WRITE setOutputs NOTIFY outputsChanged FINAL)
    Q_PROPERTY(int z READ z WRITE setZ NOTIFY zChanged FINAL)
    QML_NAMED_ELEMENT(OutputLayer)
    QML_UNCREATABLE("OutputLayer is only available via attached properties")
    QML_ATTACHED(WOutputLayer)

public:
    enum Flag {
        SizeFollowTransformation = 0x1,
        SizeFollowItemTransformation = 0x2
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

    QList<WOutputViewport*> outputs() const;
    void setOutputs(const QList<WOutputViewport*> &newOutputList);

    int z() const;
    void setZ(int newZ);

Q_SIGNALS:
    void enabledChanged();
    void flagsChanged();
    void outputsChanged();
    void zChanged();

private:
    void setAccepted(bool accepted);
    bool isAccepted() const;

    friend class OutputHelper;
};

WAYLIB_SERVER_END_NAMESPACE

Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WOutputLayer::Flags)
