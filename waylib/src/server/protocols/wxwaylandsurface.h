// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <WSurface>
#include <wtoplevelsurface.h>

QW_BEGIN_NAMESPACE
class qw_xwayland_surface;
QW_END_NAMESPACE

struct wlr_xwayland_surface;

WAYLIB_SERVER_BEGIN_NAMESPACE

class WXWayland;
class WSeat;
class WXWaylandSurfacePrivate;
class WAYLIB_SERVER_EXPORT WXWaylandSurface : public WToplevelSurface
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WXWaylandSurface)
    Q_PROPERTY(WXWaylandSurface* parentXWaylandSurface READ parentXWaylandSurface NOTIFY parentXWaylandSurfaceChanged FINAL)
    Q_PROPERTY(QList<WXWaylandSurface*> children READ children NOTIFY childrenChanged)
    Q_PROPERTY(bool isToplevel READ isToplevel NOTIFY isToplevelChanged)
    Q_PROPERTY(bool hasChild READ hasChild NOTIFY hasChildChanged)
    Q_PROPERTY(bool bypassManager READ isBypassManager NOTIFY bypassManagerChanged FINAL)
    Q_PROPERTY(QRect geometry READ geometry NOTIFY geometryChanged FINAL)
    Q_PROPERTY(WindowTypes windowTypes READ windowTypes NOTIFY windowTypesChanged FINAL)
    Q_PROPERTY(DecorationsFlags decorationsFlags READ decorationsFlags NOTIFY decorationsFlagsChanged FINAL)
    QML_NAMED_ELEMENT(XWaylandSurface)
    QML_UNCREATABLE("Only create in C++")

public:
    enum StackMode {
        XCB_STACK_MODE_ABOVE = 0,
        XCB_STACK_MODE_BELOW = 1,
        XCB_STACK_MODE_TOP_IF = 2,
        XCB_STACK_MODE_BOTTOM_IF = 3,
        XCB_STACK_MODE_OPPOSITE = 4
    };
    Q_ENUM(StackMode)

    enum ConfigureFlag {
        XCB_CONFIG_WINDOW_X = 1,
        XCB_CONFIG_WINDOW_Y = 2,
        XCB_CONFIG_WINDOW_POSITION = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
        XCB_CONFIG_WINDOW_WIDTH = 4,
        XCB_CONFIG_WINDOW_HEIGHT = 8,
        XCB_CONFIG_WINDOW_SIZE = XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT
    };
    Q_ENUM(ConfigureFlag)
    Q_DECLARE_FLAGS(ConfigureFlags, ConfigureFlag)

    enum WindowType {
        NET_WM_WINDOW_TYPE_NORMAL = 1,
        NET_WM_WINDOW_TYPE_UTILITY = 2,
        NET_WM_WINDOW_TYPE_TOOLTIP = 4,
        NET_WM_WINDOW_TYPE_DND = 8,
        NET_WM_WINDOW_TYPE_DROPDOWN_MENU = 16,
        NET_WM_WINDOW_TYPE_POPUP_MENU = 32,
        NET_WM_WINDOW_TYPE_COMBO = 64,
        NET_WM_WINDOW_TYPE_MENU = 128,
        NET_WM_WINDOW_TYPE_NOTIFICATION = 256,
        NET_WM_WINDOW_TYPE_SPLASH = 512
    };
    Q_ENUM(WindowType)
    Q_DECLARE_FLAGS(WindowTypes, WindowType)

    enum DecorationsFlag {
        DecorationsAll = 0,
        DecorationsNoBorder = 1,
        DecorationsNoTitle = 2
    };
    Q_ENUM(DecorationsFlag)
    Q_DECLARE_FLAGS(DecorationsFlags, DecorationsFlag)

    explicit WXWaylandSurface(QW_NAMESPACE::qw_xwayland_surface *handle, WXWayland *xwayland, QObject *parent = nullptr);
    ~WXWaylandSurface();

    static WXWaylandSurface *fromHandle(QW_NAMESPACE::qw_xwayland_surface *handle);
    static WXWaylandSurface *fromHandle(wlr_xwayland_surface *handle);
    static WXWaylandSurface *fromSurface(WSurface *surface);

    WSurface *surface() const override;
    QW_NAMESPACE::qw_xwayland_surface *handle() const;
    WXWaylandSurface *parentXWaylandSurface() const;
    WXWayland *xwayland() const;

    const QList<WXWaylandSurface *> &children() const;
    bool isToplevel() const;
    bool hasChild() const;
    bool isMaximized() const override;
    bool isMinimized() const override;
    bool isFullScreen() const override;
    bool isActivated() const override;

    bool hasCapability(Capability cap) const override;

    QSize minSize() const override;
    QSize maxSize() const override;
    QRect geometry() const;

    QRect getContentGeometry() const override;

    QString title() const override;
    QString appId() const override;
    [[nodiscard]] pid_t pid() const override;
    [[nodiscard]] int pidFD() const override;

    QRect requestConfigureGeometry() const;
    ConfigureFlags requestConfigureFlags() const;

    bool isBypassManager() const;
    WindowTypes windowTypes() const;
    DecorationsFlags decorationsFlags() const;

public Q_SLOTS:
    bool checkNewSize(const QSize &size, QSize *clipedSize = nullptr) override;
    void resize(const QSize &size) override;
    void configure(const QRect &geometry);
    void setMaximize(bool on) override;
    void setMinimize(bool on) override;
    void setFullScreen(bool on) override;
    void setActivate(bool on) override;
    void close() override;
    void restack(WXWaylandSurface *sibling, StackMode mode);

Q_SIGNALS:
    void parentXWaylandSurfaceChanged();
    void childrenChanged();
    void isToplevelChanged();
    void hasChildChanged();
    void bypassManagerChanged();
    void geometryChanged();
    void windowTypesChanged();
    void decorationsFlagsChanged();

    void requestConfigure(QRect geometry, ConfigureFlags flags);
    void requestActivate();
};

WAYLIB_SERVER_END_NAMESPACE

Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface::ConfigureFlags)
Q_DECLARE_OPERATORS_FOR_FLAGS(WAYLIB_SERVER_NAMESPACE::WXWaylandSurface::WindowTypes)
