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

#include <WSurface>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXdgSurfaceHandle;
class WXdgSurfacePrivate;
class WAYLIB_SERVER_EXPORT WXdgSurface : public WSurface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WXdgSurface)

public:
    explicit WXdgSurface(WXdgSurfaceHandle *handle, QObject *parent = nullptr);
    ~WXdgSurface();

    static Type *toplevelType();
    static Type *popupType();
    static Type *noneType();
    Type *type() const override;
    bool testAttribute(Attribute attr) const override;

    WXdgSurfaceHandle *handle() const;
    WSurfaceHandle *inputTargetAt(qreal scale, QPointF &globalPos) const override;

    template<typename DNativeInterface>
    inline DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }

    static WXdgSurface *fromHandle(WXdgSurfaceHandle *handle);
    template<typename DNativeInterface>
    inline static WXdgSurface *fromHandle(DNativeInterface *handle) {
        return fromHandle(reinterpret_cast<WXdgSurfaceHandle*>(handle));
    }

    bool inputRegionContains(qreal scale, const QPointF &globalPos) const override;
    WSurface *parentSurface() const override;

    bool resizeing() const;
    QPointF position() const override;

public Q_SLOTS:
    void setResizeing(bool resizeing);
    void setMaximize(bool on);
    void setActivate(bool on);
    void resize(const QSize &size) override;

protected:
    void notifyChanged(ChangeType type) override;
    void notifyBeginState(State state) override;
    void notifyEndState(State state) override;
};

WAYLIB_SERVER_END_NAMESPACE
