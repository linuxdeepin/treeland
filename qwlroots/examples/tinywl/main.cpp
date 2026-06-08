// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QProcess>
#include <QRect>
#include <QLoggingCategory>

#include <qwbackend.h>
#include <qwdisplay.h>
#include <qwrenderer.h>
#include <qwallocator.h>
#include <qwcompositor.h>
#include <qwsubcompositor.h>
#include <qwdatadevice.h>
#include <qwoutputlayout.h>
#include <qwoutput.h>
#include <qwscene.h>
#include <qwseat.h>
#include <qwxdgshell.h>
#include <qwcursor.h>
#include <qwxcursormanager.h>
#include <qwinputdevice.h>
#include <qwkeyboard.h>
#include <qwpointer.h>
#include <qwtexture.h>
#include <qwlogging.h>
#include <qwbox.h>

class Q_DECL_HIDDEN TinywlServer : public QObject
{
    Q_OBJECT
public:
    TinywlServer();
    ~TinywlServer();

    bool start();

private Q_SLOTS:
    void onNewOutput(wlr_output *output);
#if WLR_VERSION_MINOR < 18
    void onNewXdgSurface(wlr_xdg_surface *surface);
#endif
    void onNewXdgToplevel(wlr_xdg_toplevel *surface);
    void onNewXdgPopup(wlr_xdg_popup *surface);

    void onMap();
    void onUnmap();
    void onXdgToplevelRequestMove(wlr_xdg_toplevel_move_event *);
    void onXdgToplevelRequestResize(wlr_xdg_toplevel_resize_event *event);
    void onXdgToplevelRequestMaximize();
    void onXdgToplevelRequestRequestFullscreen();

    void onCursorMotion(wlr_pointer_motion_event *event);
    void onCursorMotionAbsolute(wlr_pointer_motion_absolute_event *event);
    void onCursorButton(wlr_pointer_button_event *event);
    void onCursorAxis(wlr_pointer_axis_event *event);
    void onCursorFrame();

    void onNewInput(wlr_input_device *device);
    void onRequestSetCursor(wlr_seat_pointer_request_set_cursor_event *event);
    void onRequestSetSelection(wlr_seat_request_set_selection_event *event);

    void onKeyboardModifiers();
    void onKeyboardKey(wlr_keyboard_key_event *event);
    void onKeyboardDestroy();

    void onOutputFrame();

private:
    struct View
    {
        TinywlServer *server;
        qw_xdg_toplevel *xdgToplevel;
        qw_scene_tree *sceneTree;
        QPointF pos;
    };

    enum class CursorState {
        Normal,
        MovingWindow,
        ResizingWindow,
    };

    static inline View *getView(const qw_xdg_surface *surface);
    View *viewAt(const QPointF &pos, wlr_surface **surface, QPointF *spos) const;
    void processCursorMotion(uint32_t time);
    void focusView(View *view, wlr_surface *surface);
    void beginInteractive(View *view, CursorState state, uint32_t edges);
    bool handleKeybinding(xkb_keysym_t sym);

    qw_display *display;
    qw_backend *backend;
    qw_renderer *renderer;
    qw_allocator *allocator;

    qw_compositor *compositor;
    qw_subcompositor *subcompositor;
    qw_data_device_manager *dataDeviceManager;

    qw_output_layout *outputLayout;
    QList<qw_output*> outputs;

    qw_scene *scene;
    qw_scene_output_layout *scene_layout;

    qw_xdg_shell *xdgShell;
    QList<View*> views;
    View *grabbedView = nullptr;
    QPointF grabCursorPos;
    QRectF grabGeoBox;

    qw_cursor *cursor;
    qw_xcursor_manager *cursorManager;
    CursorState cursorState = CursorState::Normal;
    uint32_t resizingEdges = 0;

    QList<qw_keyboard*> keyboards;
    qw_seat *seat;
};

TinywlServer::TinywlServer()
{
    display = new qw_display();
#if WLR_VERSION_MINOR > 17
    backend = qw_backend::autocreate(display->get_event_loop(), nullptr);
#else
    backend = qw_backend::autocreate(*display, nullptr);
#endif
    if (!backend)
        qFatal("failed to create wlr_backend");

    // Add output
   // auto multiBackend = qobject_cast<qw_multi_backend*>(backend);
   // multiBackend->for_each_backend([] (struct wlr_backend *backend, void *data) {
   //     auto x11 = qw_x11_backend::from(backend);
   //     if (x11)
   //         x11->output_create();
   // }, nullptr);

    renderer = qw_renderer::autocreate(*backend);
    if (!renderer)
        qFatal("failed to create wlr_renderer");
    renderer->init_wl_display(*display);

    allocator = qw_allocator::autocreate(*backend, *renderer);
    if (!allocator)
        qFatal("failed to create wlr_allocator");

    compositor = qw_compositor::create(*display, 4, *renderer);
    subcompositor = qw_subcompositor::create(*display);
    dataDeviceManager = qw_data_device_manager::create(*display);

#if WLR_VERSION_MAJOR == 0 && WLR_VERSION_MINOR < 18
    outputLayout = new qw_output_layout(display);
#else
    outputLayout = new qw_output_layout(*display);
#endif
    connect(backend, &qw_backend::notify_new_output, this, &TinywlServer::onNewOutput);

    scene = qw_scene::create();
    scene_layout = qw_scene_output_layout::from(scene->attach_output_layout(*outputLayout));
    xdgShell = qw_xdg_shell::create(*display, 3);
#if WLR_VERSION_MINOR > 17
    connect(xdgShell, &qw_xdg_shell::notify_new_toplevel, this, &TinywlServer::onNewXdgToplevel);
    connect(xdgShell, &qw_xdg_shell::notify_new_popup, this, &TinywlServer::onNewXdgPopup);
#else
    connect(xdgShell, &qw_xdg_shell::notify_new_surface, this, &TinywlServer::onNewXdgSurface);
#endif

    cursor = qw_cursor::create();
    cursor->attach_output_layout(*outputLayout);
    cursorManager = qw_xcursor_manager::create(nullptr, 24);
    cursorManager->load(1);
    connect(cursor, &qw_cursor::notify_motion, this, &TinywlServer::onCursorMotion);
    connect(cursor, &qw_cursor::notify_motion_absolute, this, &TinywlServer::onCursorMotionAbsolute);
    connect(cursor, &qw_cursor::notify_button, this, &TinywlServer::onCursorButton);
    connect(cursor, &qw_cursor::notify_axis, this, &TinywlServer::onCursorAxis);
    connect(cursor, &qw_cursor::notify_frame, this, &TinywlServer::onCursorFrame);

    connect(backend, &qw_backend::notify_new_input, this, &TinywlServer::onNewInput);

    seat = qw_seat::create(*display, "seat0");
    connect(seat, &qw_seat::notify_request_set_cursor, this, &TinywlServer::onRequestSetCursor);
    connect(seat, &qw_seat::notify_request_set_selection, this, &TinywlServer::onRequestSetSelection);
}

TinywlServer::~TinywlServer()
{
    delete backend;
}

bool TinywlServer::start()
{
    const char *socket = display->add_socket_auto();
    if (!socket) {
        return false;
    }

    if (!backend->start())
        return false;

    qputenv("WAYLAND_DISPLAY", QByteArray(socket));
    qInfo("Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);

    display->start(qApp->thread());
    return true;
}

void TinywlServer::onNewOutput(wlr_output *wl_output)
{
    auto output = qw_output::from(wl_output);
    Q_ASSERT(output);
    outputs.append(output);
    output->init_render(*allocator, *renderer);

    wlr_output_state state;
    wlr_output_state_init(&state);
    if (!wl_list_empty(&output->handle()->modes)) {
        auto *mode = output->preferred_mode();
        wlr_output_state_set_mode(&state, mode);
    }
    wlr_output_state_set_enabled(&state, true);
    if (!output->commit_state(&state))
        return;

    connect(output, &qw_output::notify_frame, this, &TinywlServer::onOutputFrame);
    auto l_output =outputLayout->add_auto(*output);
    auto sceneOutput = qw_scene_output::create(*scene, *output);
    scene_layout->add_output(l_output, *sceneOutput);
}

#if WLR_VERSION_MINOR < 18
void TinywlServer::onNewXdgSurface(wlr_xdg_surface *surface)
{
    if (surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
        onNewXdgPopup(surface->popup);
    } else {
        Q_ASSERT(surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL);
        onNewXdgToplevel(surface->toplevel);
    }
}
#endif

#if QT_VERSION_MAJOR < 6
Q_DECLARE_METATYPE(QMetaObject::Connection)
#endif

void TinywlServer::onNewXdgPopup(wlr_xdg_popup *popup)
{
    auto parent = qw_xdg_surface::try_from_wlr_surface(popup->parent);
    qw_scene_tree *parentTree = reinterpret_cast<qw_scene_tree*>(parent->handle()->data);
    popup->base->data = qw_scene::xdg_surface_create(*parentTree, popup->base);
#if WLR_VERSION_MINOR >= 18
    auto qwPopup = qw_xdg_popup::from(popup);
    QMetaObject::Connection initialCommitConn = connect(qw_surface::from(popup->base->surface), &qw_surface::notify_commit,
                                                        qwPopup, [qwPopup] {
        if (qwPopup->handle()->base->initial_commit) {
            qw_xdg_surface::from(qwPopup->handle()->base)->schedule_configure();
            QObject::disconnect(qvariant_cast<QMetaObject::Connection>(qwPopup->property("__initialCommitConn")));
            qwPopup->setProperty("__initialCommitConn", QVariant());
        }
    });
    qwPopup->setProperty("__initialCommitConn", QVariant::fromValue(initialCommitConn));
#endif
    return;
}

void TinywlServer::onNewXdgToplevel(wlr_xdg_toplevel *toplevel)
{
    auto view = new View();
    view->server = this;
    auto s = qw_xdg_toplevel::from(toplevel);
    view->xdgToplevel = s;
    view->sceneTree = qw_scene::xdg_surface_create(&(*scene)->tree, toplevel->base);
    view->sceneTree->handle()->node.data = view;
    toplevel->base->data = view->sceneTree;
    auto *ss = qw_surface::from(toplevel->base->surface);
    connect(ss, &qw_surface::notify_map, this, &TinywlServer::onMap);
    connect(ss, &qw_surface::notify_unmap, this, &TinywlServer::onUnmap);
    connect(s, &qw_xdg_toplevel::notify_request_move, this, &TinywlServer::onXdgToplevelRequestMove);
    connect(s, &qw_xdg_toplevel::notify_request_resize, this, &TinywlServer::onXdgToplevelRequestResize);
    connect(s, &qw_xdg_toplevel::notify_request_maximize, this, &TinywlServer::onXdgToplevelRequestMaximize);
    connect(s, &qw_xdg_toplevel::notify_request_fullscreen, this, &TinywlServer::onXdgToplevelRequestRequestFullscreen);
    connect(s, &qw_xdg_toplevel::before_destroy, this, [this, view] {
        views.removeOne(view);
        if (grabbedView == view)
            grabbedView = nullptr;
        delete view;
    });
#if WLR_VERSION_MINOR >= 18
    QMetaObject::Connection initialCommitConn = connect(qw_surface::from(toplevel->base->surface), &qw_surface::notify_commit,
                                                        s, [s] {
        if (s->handle()->base->initial_commit) {
            s->set_size(0, 0);
            QObject::disconnect(qvariant_cast<QMetaObject::Connection>(s->property("__initialCommitConn")));
            s->setProperty("__initialCommitConn", QVariant());
        }
    });
    s->setProperty("__initialCommitConn", QVariant::fromValue(initialCommitConn));
#endif
}

void TinywlServer::onMap()
{
    auto surface = qw_xdg_surface::try_from_wlr_surface(*qobject_cast<qw_surface*>(sender()));
    if (!surface)
        return;
    auto view = getView(surface);
    Q_ASSERT(view);
    views.append(view);
    focusView(view, (*surface)->surface);
}

void TinywlServer::onUnmap()
{
    auto surface = qw_xdg_surface::try_from_wlr_surface(*qobject_cast<qw_surface*>(sender()));
    if (!surface)
        return;
    auto view = getView(surface);
    Q_ASSERT(view);
    views.removeOne(view);
}

void TinywlServer::onXdgToplevelRequestMove(wlr_xdg_toplevel_move_event *)
{
    auto surface = qobject_cast<qw_xdg_toplevel*>(sender());
    auto view = getView(qw_xdg_surface::from((*surface)->base));
    Q_ASSERT(view);
    beginInteractive(view, CursorState::MovingWindow, 0);
}

void TinywlServer::onXdgToplevelRequestResize(wlr_xdg_toplevel_resize_event *event)
{
    auto surface = qobject_cast<qw_xdg_toplevel*>(sender());
    auto view = getView(qw_xdg_surface::from((*surface)->base));
    Q_ASSERT(view);
    beginInteractive(view, CursorState::ResizingWindow, event->edges);
}

void TinywlServer::onXdgToplevelRequestMaximize()
{
    auto surface = qobject_cast<qw_xdg_toplevel*>(sender());
    qw_xdg_surface::from((*surface)->base)->schedule_configure();
}

void TinywlServer::onXdgToplevelRequestRequestFullscreen()
{
    auto surface = qobject_cast<qw_xdg_toplevel*>(sender());
    qw_xdg_surface::from((*surface)->base)->schedule_configure();
}

void TinywlServer::onCursorMotion(wlr_pointer_motion_event *event)
{
    cursor->move(&event->pointer->base, event->delta_x, event->delta_y);
    processCursorMotion(event->time_msec);
}

void TinywlServer::onCursorMotionAbsolute(wlr_pointer_motion_absolute_event *event)
{
    cursor->warp_absolute(&event->pointer->base, event->x, event->y);
    processCursorMotion(event->time_msec);
}

void TinywlServer::onCursorButton(wlr_pointer_button_event *event)
{
    seat->pointer_notify_button(event->time_msec, event->button, event->state);
    QPointF spos;
    wlr_surface *surface = nullptr;
    auto view = viewAt(QPointF((*cursor)->x, (*cursor)->y), &surface, &spos);
#if WLR_VERSION_MINOR > 17
    if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
#else
    if (event->state == WLR_BUTTON_RELEASED) {
#endif
        cursorState = CursorState::Normal;
    } else {
        focusView(view, surface);
    }
}

void TinywlServer::onCursorAxis(wlr_pointer_axis_event *event)
{
    seat->pointer_notify_axis(event->time_msec, event->orientation,
                                 event->delta, event->delta_discrete, event->source
#if WLR_VERSION_MINOR > 17
                              , event->relative_direction
#endif
                              );
}

void TinywlServer::onCursorFrame()
{
    seat->pointer_notify_frame();
}

void TinywlServer::onNewInput(wlr_input_device *wl_device)
{
    auto device = qw_input_device::from(wl_device);
    if (qw_keyboard *keyboard = qobject_cast<qw_keyboard*>(device)) {

        xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        xkb_keymap *keymap = xkb_keymap_new_from_names(context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);

        keyboard->set_keymap(keymap);
        xkb_keymap_unref(keymap);
        xkb_context_unref(context);
        keyboard->set_repeat_info(25, 600);

        connect(keyboard, &qw_keyboard::notify_modifiers, this, &TinywlServer::onKeyboardModifiers);
        connect(keyboard, &qw_keyboard::notify_key, this, &TinywlServer::onKeyboardKey);
        connect(keyboard, &qw_keyboard::before_destroy, this, &TinywlServer::onKeyboardDestroy);

        seat->set_keyboard(*keyboard);

        Q_ASSERT(!keyboards.contains(keyboard));
        keyboards.append(keyboard);
    } else if (device->handle()->type == WLR_INPUT_DEVICE_POINTER) {
        cursor->attach_input_device(*device);
    }

    uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
    if (!keyboards.isEmpty()) {
        caps |= WL_SEAT_CAPABILITY_KEYBOARD;
    }
    seat->set_capabilities(caps);
}

void TinywlServer::onRequestSetCursor(wlr_seat_pointer_request_set_cursor_event *event)
{
    if (seat->handle()->pointer_state.focused_client == event->seat_client)
        cursor->set_surface(event->surface, event->hotspot_x, event->hotspot_y);
}

void TinywlServer::onRequestSetSelection(wlr_seat_request_set_selection_event *event)
{
    seat->set_selection(event->source, event->serial);
}

void TinywlServer::onKeyboardModifiers()
{
    qw_keyboard *keyboard = qobject_cast<qw_keyboard*>(QObject::sender());
    seat->set_keyboard(*keyboard);
    seat->keyboard_notify_modifiers(&keyboard->handle()->modifiers); // (*keyboard)->modifiers
}

void TinywlServer::onKeyboardKey(wlr_keyboard_key_event *event)
{
    qw_keyboard *keyboard = qobject_cast<qw_keyboard*>(QObject::sender());
    // Translate libinput keycode -> xkbcommon
    uint32_t keycode = event->keycode + 8;
    const xkb_keysym_t *syms;
    int nsyms = xkb_state_key_get_syms(keyboard->handle()->xkb_state, keycode, &syms);

    bool handled = false;
    uint32_t modifiers = keyboard->get_modifiers();
    if ((modifiers & (WLR_MODIFIER_ALT | WLR_MODIFIER_CTRL))
            && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        for (int i = 0; i < nsyms; i++)
            handled = handleKeybinding(syms[i]);
    }

    if (!handled) {
        seat->set_keyboard(*keyboard);
        seat->keyboard_notify_key(event->time_msec, event->keycode, event->state);
    }
}

void TinywlServer::onKeyboardDestroy()
{
    qw_keyboard *keyboard = qobject_cast<qw_keyboard*>(QObject::sender());
    keyboards.removeOne(keyboard);
}

void TinywlServer::onOutputFrame()
{
    auto output = qobject_cast<qw_output*>(sender());
    Q_ASSERT(output);

    auto sceneOutput = qw_scene_output::from(scene->get_scene_output(*output));
    sceneOutput->commit(nullptr);
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    sceneOutput->send_frame_done(&now);
}

TinywlServer::View *TinywlServer::getView(const qw_xdg_surface *surface)
{
    auto sceneTree = reinterpret_cast<qw_scene_tree*>(surface->handle()->data);
    return reinterpret_cast<View*>(sceneTree->handle()->node.data);
}

TinywlServer::View *TinywlServer::viewAt(const QPointF &pos, wlr_surface **surface, QPointF *spos) const
{
    double spos_x, spos_y;
    auto node = scene->at(pos.x(), pos.y(), &spos_x, &spos_y);
    spos->setX(spos_x);
    spos->setY(spos_y);
    if (!node || node->type != WLR_SCENE_NODE_BUFFER) {
        return nullptr;
    }

    auto *sceneBuffer = qw_scene_buffer::from(node);
    auto *sceneSurface = wlr_scene_surface_try_from_buffer(sceneBuffer->handle());
    if (!sceneSurface)
        return nullptr;

    *surface = sceneSurface->surface;
    wlr_scene_tree *tree = node->parent;
    while (tree && !tree->node.data) {
        tree = tree->node.parent;
    }

    return tree ? reinterpret_cast<View*>(tree->node.data) : nullptr;
}

void TinywlServer::processCursorMotion(uint32_t time)
{
    if (cursorState == CursorState::MovingWindow) {
        grabbedView->pos = grabGeoBox.topLeft() + (QPointF((*cursor)->x, (*cursor)->y) - grabCursorPos);
        grabbedView->sceneTree->set_position(grabbedView->pos.x(), grabbedView->pos.y());
        return;
    } else if (cursorState == CursorState::ResizingWindow) {
        const QPointF cursorPos((*cursor)->x, (*cursor)->y);
        QRectF newGeoBox = grabGeoBox;
        const int minimumSize = 10;

        if (resizingEdges & WLR_EDGE_TOP) {
            newGeoBox.setTop(cursorPos.y());
        } else if (resizingEdges & WLR_EDGE_BOTTOM) {
            newGeoBox.setBottom(cursorPos.y());
        }
        if (resizingEdges & WLR_EDGE_LEFT) {
            newGeoBox.setLeft(cursorPos.x());
        } else if (resizingEdges & WLR_EDGE_RIGHT) {
            newGeoBox.setRight(cursorPos.x());
        }

        QSize minSize(grabbedView->xdgToplevel->handle()->current.min_width,
                      grabbedView->xdgToplevel->handle()->current.min_height);
        QSize maxSize(grabbedView->xdgToplevel->handle()->current.max_width,
                      grabbedView->xdgToplevel->handle()->current.max_height);

        if (maxSize.width() == 0)
            maxSize.setWidth(99999);
        if (maxSize.height() == 0)
            maxSize.setHeight(99999);

        qw_box current_geo_box;
#if WLR_VERSION_MINOR < 19
        qw_xdg_surface::from(grabbedView->xdgToplevel->handle()->base)->get_geometry(current_geo_box);
#else
        current_geo_box = qw_box(qw_xdg_surface::from(grabbedView->xdgToplevel->handle()->base)->handle()->geometry);
#endif
        auto currentGeoBox = current_geo_box.toQRect();

        currentGeoBox.moveTopLeft((grabbedView->pos + currentGeoBox.topLeft()).toPoint());
        if (newGeoBox.width() < qMax(minimumSize, minSize.width()) || newGeoBox.width() > maxSize.width()) {
            newGeoBox.setLeft(currentGeoBox.left());
            newGeoBox.setRight(currentGeoBox.right());
        }

        if (newGeoBox.height() < qMax(minimumSize, minSize.height()) || newGeoBox.height() > maxSize.height()) {
            newGeoBox.setTop(currentGeoBox.top());
            newGeoBox.setBottom(currentGeoBox.bottom());
        }

        grabbedView->pos = newGeoBox.topLeft();
        const QPoint &treePosition = grabbedView->pos.toPoint();
        grabbedView->sceneTree->set_position(treePosition.x(), treePosition.y());
        const QSize &toplevelSize = newGeoBox.size().toSize();
        grabbedView->xdgToplevel->set_size(toplevelSize.width(), toplevelSize.height());
        return;
    }

    wlr_surface *surface = nullptr;
    QPointF spos;
    auto view = viewAt(QPointF((*cursor)->x, (*cursor)->y), &surface, &spos);
    if (!view)
        cursor->set_xcursor(*cursorManager, "left_ptr");
    if (surface) {
        seat->pointer_notify_enter(surface, spos.x(), spos.y());
        seat->pointer_notify_motion(time, spos.x(), spos.y());
    } else {
        seat->pointer_clear_focus();
    }
}

void TinywlServer::focusView(View *view, wlr_surface *surface)
{
    if (!view)
        return;

    wlr_surface *prevSurface = seat->handle()->keyboard_state.focused_surface;
    if (prevSurface == surface) {
        return;
    }
    if (prevSurface) {
        auto toplevel = qw_xdg_toplevel::try_from_wlr_surface(prevSurface);
        Q_ASSERT(toplevel);
        toplevel->set_activated(false);
    }

    view->sceneTree->raise_to_top();
    views.move(views.indexOf(view), 0);
    view->xdgToplevel->set_activated(true);

    if (wlr_keyboard *keyboard = seat->get_keyboard()) {
        seat->keyboard_notify_enter(view->xdgToplevel->handle()->base->surface,
                                       keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
    }
}

void TinywlServer::beginInteractive(View *view, CursorState state, uint32_t edges)
{
    wlr_surface *focusedSurface = (*seat)->pointer_state.focused_surface;
    if (view->xdgToplevel->handle()->base->surface !=
            wlr_surface_get_root_surface(focusedSurface)) {
        return;
    }
    grabbedView = view;
    cursorState = state;
    grabCursorPos = QPointF((*cursor)->x, (*cursor)->y);
    wlr_box grab_geo_box;
#if WLR_VERSION_MINOR < 19
    qw_xdg_surface::from(view->xdgToplevel->handle()->base)->get_geometry(&grab_geo_box);
#else
    grab_geo_box = qw_xdg_surface::from(view->xdgToplevel->handle()->base)->handle()->geometry;
#endif

    grabGeoBox.moveTopLeft(view->pos + QPoint(grab_geo_box.x, grab_geo_box.y));
    grabGeoBox.setWidth(grab_geo_box.width);
    grabGeoBox.setHeight(grab_geo_box.height);
    resizingEdges = edges;
}

bool TinywlServer::handleKeybinding(xkb_keysym_t sym)
{
    switch (sym) {
    case XKB_KEY_Escape:
        display->terminate();
        qApp->exit();
        break;
    case XKB_KEY_F1:
        if (views.size() < 2)
            break;

        focusView(views.at(1), views.at(1)->xdgToplevel->handle()->base->surface);
        break;
    default:
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    qw_log::init();
    QGuiApplication app(argc, argv);

    app.setApplicationName("tinywl");
    app.setApplicationVersion("0.0.1");

    QCommandLineOption startup("s", "startup command", "command");
    QCommandLineParser cl;

    cl.addOption(startup);
    cl.addHelpOption();
    cl.addVersionOption();
    cl.process(app);

    TinywlServer server;
    if (!server.start())
        return -1;

    if (cl.isSet(startup)) {
        const QString command = cl.value(startup);
        QProcess::startDetached("/bin/sh", {"-c", command});
    }

    return app.exec();
}

#include "main.moc"
