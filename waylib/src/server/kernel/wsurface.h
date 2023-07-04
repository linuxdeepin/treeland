// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wglobal.h>
#include <wtypes.h>

#include <QObject>
#include <QRect>
#include <QQmlEngine>

#include <any>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WServer;
class WTextureHandle;
class WOutput;
class WSurfaceHandler;
struct SurfaceData;
class WSurfaceHandle;
class WSurfacePrivate;
class WAYLIB_SERVER_EXPORT WSurface : public QObject, public WObject
{
    friend class WSurfaceHandler;

    Q_OBJECT
    W_DECLARE_PRIVATE(WSurface)
    Q_PROPERTY(QSize size READ size NOTIFY sizeChanged)
    Q_PROPERTY(QSize bufferSize READ bufferSize NOTIFY bufferSizeChanged)
    Q_PROPERTY(int bufferScale READ bufferScale NOTIFY bufferScaleChanged)
    Q_PROPERTY(WSurface *parentSurface READ parentSurface)
    Q_PROPERTY(QObject* shell READ shell WRITE setShell NOTIFY shellChanged)
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

    WSurfaceHandle *handle() const;
    virtual WSurfaceHandle *inputTargetAt(QPointF &globalPos) const;

    template<typename DNativeInterface>
    inline DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }
    template<typename DNativeInterface>
    inline DNativeInterface *nativeInputTargetAt(QPointF &spos) const {
        return reinterpret_cast<DNativeInterface*>(inputTargetAt(spos));
    }

    static WSurface *fromHandle(WSurfaceHandle *handle);
    template<typename DNativeInterface>
    inline static WSurface *fromHandle(DNativeInterface *handle) {
        return fromHandle(reinterpret_cast<WSurfaceHandle*>(handle));
    }

    virtual bool inputRegionContains(const QPointF &localPos) const;

    WServer *server() const;
    virtual WSurface *parentSurface() const;

    // for current state
    QSize size() const;
    QSize bufferSize() const;
    WLR::Transform orientation() const;
    int bufferScale() const;

    virtual void resize(const QSize &newSize);

    QPointF mapToGlobal(const QPointF &localPos) const;
    QPointF mapFromGlobal(const QPointF &globalPos) const;

    WTextureHandle *texture() const;
    QPoint textureOffset() const;

    void notifyFrameDone();

    void enterOutput(WOutput *output);
    void leaveOutput(WOutput *output);
    QVector<WOutput*> outputs() const;

    virtual QPointF position() const;
    WSurfaceHandler *handler() const;
    void setHandler(WSurfaceHandler *handler);

    virtual void notifyChanged(ChangeType, std::any oldValue, std::any newValue);
    virtual void notifyBeginState(State);
    virtual void notifyEndState(State);

    QObject *shell() const;
    void setShell(QObject *shell);

Q_SIGNALS:
    void requestMap();
    void requestUnmap();

    void textureChanged();
    void sizeChanged(QSize oldSize, QSize newSize);
    void bufferSizeChanged(QSize oldSize, QSize newSize);
    void bufferScaleChanged(int oldScale, int newScale);
    void shellChanged();

protected:
    WSurface(WSurfacePrivate &dd, QObject *parent);

    void setHandle(WSurfaceHandle *handle);
};

WAYLIB_SERVER_END_NAMESPACE

Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WSurface::Attributes)
