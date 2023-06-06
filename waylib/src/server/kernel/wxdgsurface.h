// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

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
    explicit WXdgSurface(WXdgSurfaceHandle *handle, WServer *server, QObject *parent = nullptr);
    ~WXdgSurface();

    static Type *toplevelType();
    static Type *popupType();
    static Type *noneType();
    Type *type() const override;
    bool testAttribute(Attribute attr) const override;

    WXdgSurfaceHandle *handle() const;
    WSurfaceHandle *inputTargetAt(QPointF &localPos) const override;

    template<typename DNativeInterface>
    inline DNativeInterface *nativeInterface() const {
        return reinterpret_cast<DNativeInterface*>(handle());
    }

    static WXdgSurface *fromHandle(WXdgSurfaceHandle *handle);
    template<typename DNativeInterface>
    inline static WXdgSurface *fromHandle(DNativeInterface *handle) {
        return fromHandle(reinterpret_cast<WXdgSurfaceHandle*>(handle));
    }

    bool inputRegionContains(const QPointF &localPos) const override;
    WSurface *parentSurface() const override;

    bool resizeing() const;
    QPointF position() const override;

public Q_SLOTS:
    void setResizeing(bool resizeing);
    void setMaximize(bool on);
    void setActivate(bool on);
    void resize(const QSize &size) override;

protected:
    void notifyChanged(ChangeType type, std::any oldValue, std::any newValue) override;
    void notifyBeginState(State state) override;
    void notifyEndState(State state) override;
};

WAYLIB_SERVER_END_NAMESPACE
