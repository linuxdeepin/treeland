// Copyright (C) 2022 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwobject.h>

extern "C" {
#include <math.h>
#define static
#include <wlr/types/wlr_scene.h>
#undef static
#include <wlr/util/box.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(scene_node)
{
    QW_OBJECT
    Q_OBJECT

public:
    inline static qw_scene_node *create(HandleType *handle);

    QW_FUNC_MEMBER(scene_node, set_enabled, void, bool enabled)
    QW_FUNC_MEMBER(scene_node, set_position, void, int x, int y)
    QW_FUNC_MEMBER(scene_node, place_above, void, wlr_scene_node *sibling)
    QW_FUNC_MEMBER(scene_node, place_below, void, wlr_scene_node *sibling)
    QW_FUNC_MEMBER(scene_node, raise_to_top, void)
    QW_FUNC_MEMBER(scene_node, lower_to_bottom, void)
    QW_FUNC_MEMBER(scene_node, reparent, void, wlr_scene_tree *new_parent)
    QW_FUNC_MEMBER(scene_node, coords, bool, int *lx, int *ly)
    QW_FUNC_MEMBER(scene_node, for_each_buffer, void, wlr_scene_buffer_iterator_func_t iterator, void *user_data)
    QW_FUNC_MEMBER(scene_node, at, wlr_scene_node *, double lx, double ly, double *nx, double *ny)
    QW_FUNC_MEMBER(scene, subsurface_tree_set_clip, void, wlr_box *clip)

protected:
    QW_FUNC_MEMBER(scene_node, destroy, void)
};

class QW_CLASS_OBJECT(scene_surface)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(scene_surface, create, qw_scene_surface *, wlr_scene_tree *parent, wlr_surface *surface)
    QW_FUNC_STATIC(scene_surface, try_from_buffer, qw_scene_surface *, wlr_scene_buffer *scene_buffer)
};

class QW_CLASS_OBJECT(scene_output)
{
    QW_OBJECT
    Q_OBJECT

public:
    QW_FUNC_STATIC(scene_output, create, qw_scene_output *, wlr_scene *scene, wlr_output *output)
    QW_FUNC_STATIC(scene_output, layout_add_output, void, wlr_scene_output_layout *sol, wlr_output_layout_output *lo, wlr_scene_output *so)

    QW_FUNC_MEMBER(scene_output, set_position, void, int lx, int ly)
    QW_FUNC_MEMBER(scene_output, commit, bool, const wlr_scene_output_state_options *options)
    QW_FUNC_MEMBER(scene_output, build_state, bool, wlr_output_state *state, const wlr_scene_output_state_options *options)
    QW_FUNC_MEMBER(scene_output, send_frame_done, void, timespec *now)
    QW_FUNC_MEMBER(scene_output, for_each_buffer, void, wlr_scene_buffer_iterator_func_t iterator, void *user_data)

protected:
    QW_FUNC_MEMBER(scene_output, destroy, void)
};

class QW_CLASS_REINTERPRET_CAST(scene_output_layout)
{
public:
    QW_FUNC_MEMBER(scene_output_layout, add_output, void, wlr_output_layout_output *lo, wlr_scene_output *so);
};

#define QW_SCENE_NODE(name) \
typedef wlr_##name HandleType; \
typedef qw_##name DeriveType; \
public: \
    QW_FUNC_STATIC(name, from_node, qw_##name*, wlr_scene_node*) \
    QW_ALWAYS_INLINE wlr_##name *handle() const { \
       return wlr_##name##_from_node(qw_scene_node::handle()); \
    } \
    QW_ALWAYS_INLINE operator wlr_##name* () const { \
        return handle(); \
    } \
    QW_ALWAYS_INLINE static qw_##name *from(wlr_scene_node *handle) { \
        return qobject_cast<qw_##name *>(qw_scene_node::from(handle)); \
    } \
    QW_ALWAYS_INLINE static qw_##name *from(wlr_##name *handle) { \
        return from(&handle->node); \
    } \
    QW_ALWAYS_INLINE HandleType *operator -> () const { \
        return handle(); \
    } \
protected: \
qw_##name(wlr_##name *handle, bool is_own): qw_scene_node(&handle->node, is_own) { };\
private: \
friend class qw_scene_node;

class QW_EXPORT qw_scene_tree : public qw_scene_node
{
    Q_OBJECT
    QW_SCENE_NODE(scene_tree)

public:
    QW_FUNC_STATIC(scene_tree, create, qw_scene_tree *, wlr_scene_tree *parent)
    QW_FUNC_STATIC(scene, subsurface_tree_create, qw_scene_tree *, wlr_scene_tree *parent, wlr_surface *surface)
    QW_FUNC_STATIC(scene, xdg_surface_create, qw_scene_tree *, wlr_scene_tree *parent, wlr_xdg_surface *xdg_surface)
    QW_FUNC_STATIC(scene, drag_icon_create, qw_scene_tree *, wlr_scene_tree *parent, wlr_drag_icon *drag_icon)
};

class QW_EXPORT qw_scene : public qw_scene_tree
{
    Q_OBJECT
    typedef wlr_scene HandleType;
    typedef qw_scene DeriveType;

protected:
    qw_scene(wlr_scene *handle, bool is_own): qw_scene_tree(&handle->tree, is_own) { };\
private:
    friend class qw_scene_tree;

public:
    QW_ALWAYS_INLINE wlr_scene *handle() const {
       wlr_scene *handle = wl_container_of(qw_scene_tree::handle(), handle, tree);
       return handle;
    }
    QW_ALWAYS_INLINE operator wlr_scene* () const {
        return handle();
    }
    QW_ALWAYS_INLINE HandleType *operator -> () const {
        return handle();
    }

    QW_FUNC_STATIC(scene, create, qw_scene *, void)
#if WLR_VERSION_MINOR<18
    QW_FUNC_MEMBER(scene, set_presentation, void, wlr_presentation *presentation)
#endif
    QW_FUNC_MEMBER(scene, set_linux_dmabuf_v1, void, wlr_linux_dmabuf_v1 *linux_dmabuf_v1)
    QW_FUNC_MEMBER(scene, get_scene_output, wlr_scene_output *, wlr_output *output)
    QW_FUNC_MEMBER(scene, attach_output_layout, wlr_scene_output_layout *, wlr_output_layout *output_layout)
};

class qw_scene_timer
{
public:
    QW_FUNC_MEMBER(scene_timer, get_duration_ns, int64_t)
    QW_FUNC_MEMBER(scene_timer, finish, void)
};

class QW_CLASS_REINTERPRET_CAST(scene_layer_surface_v1)
{
public:
    QW_FUNC_STATIC(scene_layer_surface_v1, create, qw_scene_layer_surface_v1 *, wlr_scene_tree *parent, wlr_layer_surface_v1 *layer_surface)

    QW_FUNC_MEMBER(scene_layer_surface_v1, configure, void, const wlr_box *full_area, wlr_box *usable_area)
};

class QW_EXPORT qw_scene_rect : public qw_scene_node
{
    Q_OBJECT
    QW_SCENE_NODE(scene_rect)

public:
    QW_FUNC_STATIC(scene_rect, create, qw_scene_rect *, wlr_scene_tree *parent, int width, int height, const float color[4])

    QW_FUNC_MEMBER(scene_rect, set_size, void, int width, int height)
    QW_FUNC_MEMBER(scene_rect, set_color, void, const float color[4])
};

class QW_EXPORT qw_scene_buffer : public qw_scene_node
{
    Q_OBJECT
    QW_SCENE_NODE(scene_buffer)

    QW_SIGNAL(outputs_update, wlr_scene_outputs_update_event*)
    QW_SIGNAL(output_enter, wlr_scene_output*)
    QW_SIGNAL(output_leave, wlr_scene_output*)
    QW_SIGNAL(output_sample, wlr_scene_output_sample_event*)
    QW_SIGNAL(frame_done, timespec*)

public:
    QW_FUNC_STATIC(scene_buffer, create, qw_scene_buffer *, wlr_scene_tree *parent, wlr_buffer *buffer)

    QW_FUNC_MEMBER(scene_buffer, set_buffer, void, wlr_buffer *buffer)
    QW_FUNC_MEMBER(scene_buffer, set_buffer_with_damage, void, wlr_buffer *buffer, const pixman_region32_t *region)
    QW_FUNC_MEMBER(scene_buffer, set_opaque_region, void, const pixman_region32_t *region)
    QW_FUNC_MEMBER(scene_buffer, set_source_box, void, const wlr_fbox *box)
    QW_FUNC_MEMBER(scene_buffer, set_dest_size, void, int width, int height)
    QW_FUNC_MEMBER(scene_buffer, set_transform, void, enum wl_output_transform transform)
    QW_FUNC_MEMBER(scene_buffer, set_opacity, void, float opacity)
    QW_FUNC_MEMBER(scene_buffer, set_filter_mode, void, enum wlr_scale_filter_mode filter_mode)
    QW_FUNC_MEMBER(scene_buffer, send_frame_done, void, timespec *now)
};

qw_scene_node *qw_scene_node::create(HandleType *handle) {
    if (auto o = get(handle))
        return o;

    switch (handle->type) {
    case WLR_SCENE_NODE_RECT:
        return new qw_scene_rect(reinterpret_cast<wlr_scene_rect*>(handle), false);
    case WLR_SCENE_NODE_TREE:
        return new qw_scene_tree(reinterpret_cast<wlr_scene_tree*>(handle), false);
    case WLR_SCENE_NODE_BUFFER:
        return new qw_scene_buffer(reinterpret_cast<wlr_scene_buffer*>(handle), false);
    default:
        // Here is not reachable
        qCritical("Unknow scene node type!");
        return nullptr;
    }
}

QW_END_NAMESPACE
