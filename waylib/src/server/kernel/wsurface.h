// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wtypes.h>
#include <qwglobal.h>

#include <QObject>
#include <QRect>
#include <QQmlEngine>

#include <any>

QW_BEGIN_NAMESPACE
class QWTexture;
class QWSurface;
class QWBuffer;
QW_END_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class WServer;
class WOutput;
struct SurfaceData;
class WSurfaceHandler;
class WSurfacePrivate;
class WAYLIB_SERVER_EXPORT WSurface : public QObject, public WObject
{
    friend class WSurfaceHandler;

    Q_OBJECT
    W_DECLARE_PRIVATE(WSurface)
    Q_PROPERTY(bool mapped READ mapped NOTIFY mappedChanged)
    Q_PROPERTY(QSize size READ size NOTIFY sizeChanged)
    Q_PROPERTY(QSize bufferSize READ bufferSize NOTIFY bufferSizeChanged)
    Q_PROPERTY(int bufferScale READ bufferScale NOTIFY bufferScaleChanged)
    Q_PROPERTY(WSurface *parentSurface READ parentSurface CONSTANT)
    Q_PROPERTY(QObject* shell READ shell WRITE setShell NOTIFY shellChanged)
    Q_PROPERTY(WOutput* primaryOutput READ primaryOutput NOTIFY primaryOutputChanged)
    QML_NAMED_ELEMENT(WaylandSurface)
    QML_UNCREATABLE("Using in C++")

public:
    explicit WSurface(WServer *server, QObject *parent = nullptr);

    enum class ChangeType {
        Handler,
        Outputs,
    };
    enum class State {
        Move,
        Resize,
        Maximize,
        Activate
    };
    enum class Attribute {
        Immovable = 1 << 0,
        DoesNotAcceptFocus = 1 << 1
    };
    Q_DECLARE_FLAGS(Attributes, Attribute)
    Q_FLAG(Attribute)

    struct Type {};
    virtual Type *type() const;
    virtual bool testAttribute(Attribute attr) const;

    QW_NAMESPACE::QWSurface *handle() const;
    virtual QW_NAMESPACE::QWSurface *inputTargetAt(QPointF &globalPos) const;

    static WSurface *fromHandle(QW_NAMESPACE::QWSurface *handle);

    virtual bool inputRegionContains(const QPointF &localPos) const;

    WServer *server() const;
    virtual WSurface *parentSurface() const;

    // for current state
    bool mapped() const;
    QSize size() const;
    QSize bufferSize() const;
    WLR::Transform orientation() const;
    int bufferScale() const;

    virtual void resize(const QSize &newSize);

    QPointF mapToGlobal(const QPointF &localPos) const;
    QPointF mapFromGlobal(const QPointF &globalPos) const;

    QW_NAMESPACE::QWTexture *texture() const;
    QW_NAMESPACE::QWBuffer *buffer() const;
    QPoint textureOffset() const;

    void notifyFrameDone();

    Q_SLOT void enterOutput(WOutput *output);
    Q_SLOT void leaveOutput(WOutput *output);
    Q_SLOT QVector<WOutput*> outputs() const;
    WOutput *primaryOutput() const;

    virtual QPointF position() const;
    WSurfaceHandler *handler() const;
    void setHandler(WSurfaceHandler *handler);

    virtual void notifyChanged(ChangeType, std::any oldValue, std::any newValue);
    virtual void notifyBeginState(State);
    virtual void notifyEndState(State);

    QObject *shell() const;
    void setShell(QObject *shell);

Q_SIGNALS:
    void primaryOutputChanged();

    void mappedChanged();
    void textureChanged();
    void sizeChanged(QSize oldSize, QSize newSize);
    void bufferSizeChanged(QSize oldSize, QSize newSize);
    void bufferScaleChanged(int oldScale, int newScale);
    void shellChanged();

protected:
    WSurface(WSurfacePrivate &dd, QObject *parent);

    void setHandle(QW_NAMESPACE::QWSurface *handle);
};

WAYLIB_SERVER_END_NAMESPACE

Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WSurface::Attributes)
