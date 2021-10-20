/*
 * Copyright (C) 2021 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <wglobal.h>
#include <wtypes.h>

#include <QObject>
#include <QRect>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WServer;
class WSeat;
class WTextureHandle;
class WOutput;
class WSurfaceLayout;
class WSurfaceHandle;
class WSurfacePrivate;
class WAYLIB_SERVER_EXPORT WSurface : public QObject, public WObject
{
    friend class WSurfaceLayout;
    friend class WSurfaceLayoutPrivate;

    Q_OBJECT
    W_DECLARE_PRIVATE(WSurface)
    Q_PROPERTY(QPointF effectivePosition READ effectivePosition NOTIFY effectivePositionChanged)
    Q_PROPERTY(QPointF position READ position NOTIFY positionChanged)
    Q_PROPERTY(QSizeF effectiveSize READ effectiveSize NOTIFY effectiveSizeChanged)
    Q_PROPERTY(QSize size READ size NOTIFY sizeChanged)
    Q_PROPERTY(QSize bufferSize READ bufferSize)
    Q_PROPERTY(WSurface *parentSurface READ parentSurface)

public:
    explicit WSurface(QObject *parent = nullptr);

    enum class ChangeType {
        Layout,
        Position,
        Outputs,
        AttachedOutput
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
    virtual WSurfaceHandle *inputTargetAt(qreal scale, QPointF &globalPos) const;

    template<typename DNativeInterface>
    inline DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }
    template<typename DNativeInterface>
    inline DNativeInterface *nativeInputTargetAt(qreal scale, QPointF &spos) const {
        return reinterpret_cast<DNativeInterface*>(inputTargetAt(scale, spos));
    }

    static WSurface *fromHandle(WSurfaceHandle *handle);
    template<typename DNativeInterface>
    inline static WSurface *fromHandle(DNativeInterface *handle) {
        return fromHandle(reinterpret_cast<WSurfaceHandle*>(handle));
    }

    virtual bool inputRegionContains(qreal scale, const QPointF &globalPos) const;

    WServer *server() const;
    virtual WSurface *parentSurface() const;

    // for current state
    QSize size() const;
    QSizeF effectiveSize() const;
    QSize bufferSize() const;
    WLR::Transform orientation() const;
    int scale() const;

    virtual void resize(const QSize &newSize);

    inline QSizeF toEffectiveSize(qreal scale, const QSizeF &size) const
    { return this->scale() < scale ? size / scale : size; }
    inline QSizeF fromEffectiveSize(qreal scale, const QSizeF &size) const
    { return this->scale() < scale ? size * scale : size; }
    inline QPointF toEffectivePos(qreal scale, const QPointF &localPos) const
    { return this->scale() < scale ? localPos / scale : localPos; }
    inline QPointF fromEffectivePos(qreal scale, const QPointF &localPos) const
    { return this->scale() < scale ? localPos * scale : localPos; }

    QPointF positionToGlobal(const QPointF &localPos) const;
    QPointF positionFromGlobal(const QPointF &globalPos) const;

    WTextureHandle *texture() const;
    QPoint textureOffset() const;

    void notifyFrameDone();

    void enterOutput(WOutput *output);
    void leaveOutput(WOutput *output);
    QVector<WOutput*> currentOutputs() const;
    WOutput *attachedOutput() const;

    virtual QPointF position() const;
    QPointF effectivePosition() const;
    WSurfaceLayout *layout() const;
    void setLayout(WSurfaceLayout *layout);

    virtual void notifyChanged(ChangeType);
    virtual void notifyBeginState(State);
    virtual void notifyEndState(State);

Q_SIGNALS:
    void textureChanged();
    void positionChanged();
    void effectivePositionChanged();
    void sizeChanged();
    void effectiveSizeChanged();

protected:
    WSurface(WSurfacePrivate &dd, QObject *parent);

    void setHandle(WSurfaceHandle *handle);
};

WAYLIB_SERVER_END_NAMESPACE

Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WSurface::Attributes)
