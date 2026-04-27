// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwseat.h>
#include <wserver.h>
#include <wsurface.h>
#include <qwoutput.h>
#include <qwcompositor.h>

class SurfaceWrapper;

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

class ForeignToplevelManagerInterfaceV1Private;

struct treeland_dock_preview_context_v1_preview_event
{
    std::vector<uint32_t> toplevels;
    int32_t x, y;
    int32_t direction;
};

struct treeland_dock_preview_tooltip_event
{
    QString tooltip;
    int32_t x, y;
    int32_t direction;
};

class DockPreviewContextV1Private;
class DockPreviewContextV1 : public QObject
{
    Q_OBJECT
public:
    ~DockPreviewContextV1() override;

    wl_resource *resource() const;
    WSurface *relativeSurface() const;

    void enter();
    void leave();

    static DockPreviewContextV1 *get(wl_resource *resource);
    static DockPreviewContextV1 *getDockPreviewContext(WSurface *relativeSurface);
Q_SIGNALS:
    void requestShow(treeland_dock_preview_context_v1_preview_event *event);
    void requestShowTooltip(treeland_dock_preview_tooltip_event *event);
    void requestClose();
    void beforeDestroy();

private:
    explicit DockPreviewContextV1(wl_resource *resource, wlr_surface *_relativeSurface);

private:
    std::unique_ptr<DockPreviewContextV1Private> d;

    friend class ForeignToplevelManagerInterfaceV1Private;
};

struct treeland_foreign_toplevel_handle_v1_maximized_event
{
    bool maximized;
};

struct treeland_foreign_toplevel_handle_v1_minimized_event
{
    bool minimized;
};

struct treeland_foreign_toplevel_handle_v1_activated_event
{
    wlr_seat *seat;
};

struct treeland_foreign_toplevel_handle_v1_fullscreen_event
{
    bool fullscreen;
    wlr_output *output;
};

struct treeland_foreign_toplevel_handle_v1_set_rectangle_event
{
    wlr_surface *surface;
    int32_t x, y, width, height;
};

class ForeignToplevelHandleV1;
struct treeland_foreign_toplevel_handle_v1_output
{
    QW_NAMESPACE::qw_output *output{ nullptr };
    ForeignToplevelHandleV1 *toplevel{ nullptr };
};

class ForeignToplevelHandleV1Private;
class ForeignToplevelManagerInterfaceV1;
class ForeignToplevelHandleV1 : public QObject
{
    Q_OBJECT
public:
    enum class State
    {
        Maximized = 1,
        Minimized = 2,
        Activated = 4,
        Fullscreen = 8,
        Attention = 16,
    };
    Q_ENUM(State);
    Q_DECLARE_FLAGS(States, State)

    ~ForeignToplevelHandleV1() override;

    wl_resource *resource() const;

    void set_title(const QString &title);
    void set_app_id(const QString &app_id);
    void set_pid(const pid_t pid);
    void set_identifier(uint32_t identifier);
    uint32_t identifier() const;
    void output_enter(QW_NAMESPACE::qw_output *output);
    void output_leave(QW_NAMESPACE::qw_output *output);

    void set_maximized(bool maximized);
    void set_minimized(bool minimized);
    void set_activated(bool activated);
    void set_fullscreen(bool fullscreen);
    void set_attention(bool attention);
    void set_parent(ForeignToplevelHandleV1 *parent);

    void reset_idle_source();
    void send_done();
    void send_closed();
    void send_state();
    void send_output(qw_output *output, bool enter);

    static ForeignToplevelHandleV1 *get(wl_resource *resource);
Q_SIGNALS:
    void requestMaximize(treeland_foreign_toplevel_handle_v1_maximized_event *event);
    void requestMinimize(treeland_foreign_toplevel_handle_v1_minimized_event *event);
    void requestActivate(treeland_foreign_toplevel_handle_v1_activated_event *event);
    void requestFullscreen(treeland_foreign_toplevel_handle_v1_fullscreen_event *event);
    void requestClose();
    void rectangleChanged(treeland_foreign_toplevel_handle_v1_set_rectangle_event *event);

private:
    explicit ForeignToplevelHandleV1(ForeignToplevelManagerInterfaceV1 *manager, wl_resource *resource);
    void update_idle_source();

private:
    std::unique_ptr<ForeignToplevelHandleV1Private> d;

    friend class ForeignToplevelManagerInterfaceV1;
    friend class ForeignToplevelManagerInterfaceV1Private;
};

class ForeignToplevelManagerInterfaceV1
    : public QObject
    , public WServerInterface
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

public:
    enum class PreviewDirection
    {
        top = 0,
        right,
        bottom,
        left,
    };
    Q_ENUM(PreviewDirection);

    explicit ForeignToplevelManagerInterfaceV1(QObject *parent = nullptr);
    ~ForeignToplevelManagerInterfaceV1() override;

    void addSurface(SurfaceWrapper *wrapper);
    void removeSurface(SurfaceWrapper *wrapper);
    void releaseHandle(ForeignToplevelHandleV1 *handle);

    Q_INVOKABLE void enterDockPreview(WSurface *relativeSurface);
    Q_INVOKABLE void leaveDockPreview(WSurface *relativeSurface);

    wl_event_loop *eventLoop() const;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 1;
Q_SIGNALS:
    void requestDockPreview(std::vector<SurfaceWrapper *> surfaces,
                            WSurface *target,
                            QPoint abs,
                            PreviewDirection direction);
    void requestDockPreviewTooltip(QString tooltip,
                                   WSurface *target,
                                   QPoint abs,
                                   PreviewDirection direction);
    void requestDockClose();

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;
    void initializeToplevelHandle(SurfaceWrapper *wrapper, ForeignToplevelHandleV1 *handle);

private Q_SLOTS:
    void handleDockPreviewShow(treeland_dock_preview_context_v1_preview_event *event);
    void handleDockPreviewShowTooltip(treeland_dock_preview_tooltip_event *event);

private:
    std::unique_ptr<ForeignToplevelManagerInterfaceV1Private> d;

    friend class ForeignToplevelManagerInterfaceV1Private;
};
