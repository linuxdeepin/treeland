#pragma once

#include <qwobject.h>

extern "C" {
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/interfaces/wlr_ext_image_capture_source_v1.h>
}

QW_BEGIN_NAMESPACE

class QW_CLASS_OBJECT(ext_image_capture_source_v1)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(constraints_update)
    QW_SIGNAL(frame, wlr_ext_image_capture_source_v1_frame_event*)

public:
    QW_FUNC_STATIC(ext_image_capture_source_v1, from_resource, qw_ext_image_capture_source_v1 *, wl_resource *resource)

    QW_FUNC_MEMBER(ext_image_capture_source_v1, create_resource, bool, wl_client *client, uint32_t new_id)
    QW_FUNC_MEMBER(ext_image_capture_source_v1, set_constraints_from_swapchain, bool, wlr_swapchain *swapchain, wlr_renderer *renderer)
    QW_FUNC_MEMBER(ext_image_capture_source_v1, init, void, const wlr_ext_image_capture_source_v1_interface *impl)

protected:
    QW_FUNC_MEMBER(ext_image_capture_source_v1, finish, void)
};

class QW_CLASS_OBJECT(ext_image_capture_source_v1_cursor)
{
    QW_OBJECT
    Q_OBJECT

    QW_SIGNAL(update)

public:
    QW_FUNC_MEMBER(ext_image_capture_source_v1_cursor, init, void, const wlr_ext_image_capture_source_v1_interface *impl)

protected:
    QW_FUNC_MEMBER(ext_image_capture_source_v1_cursor, finish, void)
};

class QW_CLASS_REINTERPRET_CAST(ext_output_image_capture_source_manager_v1)
{
public:
    QW_FUNC_STATIC(ext_output_image_capture_source_manager_v1, create, qw_ext_output_image_capture_source_manager_v1 *, wl_display *display, uint32_t version)
};

QW_END_NAMESPACE
