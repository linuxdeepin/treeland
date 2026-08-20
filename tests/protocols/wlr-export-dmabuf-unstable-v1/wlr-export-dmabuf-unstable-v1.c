// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "client-connection.h"
#include "xdg-toplevel-client.h"
#include "wlr-export-dmabuf-unstable-v1-client-protocol.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef TREELAND_PROTOCOL_EXPECT_DMABUF_READY
#include <fcntl.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

struct export_events {
    unsigned int frame_count;
    unsigned int object_count;
    unsigned int ready_count;
    unsigned int cancel_count;
    uint32_t width;
    uint32_t height;
    uint32_t flags;
    uint32_t object_total;
    uint32_t object_mask;
    uint32_t cancel_reason;
    uint32_t tv_nsec;
    uint32_t format;
    uint32_t modifier_high;
    uint32_t modifier_low;
    int ordering_valid;
    int object_payload_valid;
#ifdef TREELAND_PROTOCOL_EXPECT_DMABUF_READY
    int object_fds[4];
    uint32_t object_offsets[4];
    uint32_t object_strides[4];
#endif
};

static void frame_handle_frame(void *data, struct zwlr_export_dmabuf_frame_v1 *frame,
                               uint32_t width, uint32_t height, uint32_t offset_x,
                               uint32_t offset_y, uint32_t buffer_flags, uint32_t flags,
                               uint32_t format, uint32_t mod_high, uint32_t mod_low,
                               uint32_t num_objects)
{
    (void)frame;
    (void)offset_x;
    (void)offset_y;
    (void)buffer_flags;
    struct export_events *events = data;
    events->ordering_valid = events->frame_count == 0 && events->object_count == 0
        && events->ready_count == 0 && events->cancel_count == 0;
    events->object_payload_valid = width > 0 && height > 0 && num_objects > 0 && num_objects <= 4;
    events->width = width;
    events->height = height;
    events->flags = flags;
    events->format = format;
    events->modifier_high = mod_high;
    events->modifier_low = mod_low;
    events->object_total = num_objects;
    events->frame_count++;
}

static void frame_handle_object(void *data, struct zwlr_export_dmabuf_frame_v1 *frame,
                                uint32_t index, int32_t fd, uint32_t size,
                                uint32_t offset, uint32_t stride, uint32_t plane_index)
{
    (void)frame;
#ifndef TREELAND_PROTOCOL_EXPECT_DMABUF_READY
    (void)offset;
#endif
    struct export_events *events = data;
#ifdef TREELAND_PROTOCOL_EXPECT_DMABUF_READY
    const int duplicate_fd = fd >= 0 ? fcntl(fd, F_DUPFD_CLOEXEC, 3) : -1;
#endif
    if (fd >= 0)
        close(fd);
    if (events->frame_count != 1 || events->ready_count != 0 || events->cancel_count != 0
        || index >= events->object_total || index >= 32 || (events->object_mask & (1u << index))
        || fd < 0 || size == 0 || stride == 0 || plane_index >= events->object_total
#ifdef TREELAND_PROTOCOL_EXPECT_DMABUF_READY
        || duplicate_fd < 0
#endif
        ) {
        events->object_payload_valid = 0;
#ifdef TREELAND_PROTOCOL_EXPECT_DMABUF_READY
        if (duplicate_fd >= 0)
            close(duplicate_fd);
#endif
    } else {
        events->object_mask |= 1u << index;
#ifdef TREELAND_PROTOCOL_EXPECT_DMABUF_READY
        events->object_fds[index] = duplicate_fd;
        events->object_offsets[index] = offset;
        events->object_strides[index] = stride;
#endif
    }
    events->object_count++;
}

#ifdef TREELAND_PROTOCOL_EXPECT_DMABUF_READY
static int has_extension(const char *extensions, const char *extension)
{
    const size_t extension_length = strlen(extension);
    const char *candidate = extensions;
    while (candidate && *candidate) {
        candidate = strstr(candidate, extension);
        if (!candidate)
            return 0;
        const char before = candidate == extensions ? ' ' : candidate[-1];
        const char after = candidate[extension_length];
        if ((before == ' ' || before == '\0') && (after == ' ' || after == '\0'))
            return 1;
        candidate += extension_length;
    }
    return 0;
}

static GLuint compile_shader(GLenum type, const char *source)
{
    const GLuint shader = glCreateShader(type);
    if (!shader)
        return 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled)
        return shader;
    glDeleteShader(shader);
    return 0;
}

/* Returns 1 for a verified red sample, 0 for an assertion failure, and -1
 * when the GPU runner lacks the EGL/GLES import capability needed for V. */
static int readback_exported_red(const struct export_events *events)
{
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    GLuint source_texture = 0;
    GLuint target_texture = 0;
    GLuint framebuffer = 0;
    GLuint vertex_shader = 0;
    GLuint fragment_shader = 0;
    GLuint program = 0;
    int result = -1;

    PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (get_platform_display)
        display = get_platform_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    if (display == EGL_NO_DISPLAY)
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY || !eglInitialize(display, NULL, NULL)
        || !eglBindAPI(EGL_OPENGL_ES_API))
        goto cleanup;

    const char *egl_extensions = eglQueryString(display, EGL_EXTENSIONS);
    const uint64_t modifier = ((uint64_t)events->modifier_high << 32) | events->modifier_low;
    const int needs_modifier_attributes = modifier != UINT64_MAX;
    if (!egl_extensions || !has_extension(egl_extensions, "EGL_EXT_image_dma_buf_import")
        || (needs_modifier_attributes
            && !has_extension(egl_extensions, "EGL_EXT_image_dma_buf_import_modifiers")))
        goto cleanup;

    const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE,
    };
    EGLConfig config;
    EGLint config_count = 0;
    if (!eglChooseConfig(display, config_attributes, &config, 1, &config_count) || !config_count)
        goto cleanup;
    const EGLint pbuffer_attributes[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    surface = eglCreatePbufferSurface(display, config, pbuffer_attributes);
    const EGLint context_attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
    if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT
        || !eglMakeCurrent(display, surface, surface, context))
        goto cleanup;

    const char *gl_extensions = (const char *)glGetString(GL_EXTENSIONS);
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_texture =
        (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if (!gl_extensions || !has_extension(gl_extensions, "GL_OES_EGL_image_external")
        || !image_target_texture)
        goto cleanup;

    EGLint attributes[64];
    int attribute_count = 0;
    attributes[attribute_count++] = EGL_WIDTH;
    attributes[attribute_count++] = (EGLint)events->width;
    attributes[attribute_count++] = EGL_HEIGHT;
    attributes[attribute_count++] = (EGLint)events->height;
    attributes[attribute_count++] = EGL_LINUX_DRM_FOURCC_EXT;
    attributes[attribute_count++] = (EGLint)events->format;
    static const EGLint fd_attributes[] = {
        EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE1_FD_EXT,
        EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE3_FD_EXT,
    };
    static const EGLint offset_attributes[] = {
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT,
        EGL_DMA_BUF_PLANE2_OFFSET_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT,
    };
    static const EGLint pitch_attributes[] = {
        EGL_DMA_BUF_PLANE0_PITCH_EXT, EGL_DMA_BUF_PLANE1_PITCH_EXT,
        EGL_DMA_BUF_PLANE2_PITCH_EXT, EGL_DMA_BUF_PLANE3_PITCH_EXT,
    };
    static const EGLint modifier_lo_attributes[] = {
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
        EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT,
    };
    static const EGLint modifier_hi_attributes[] = {
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
        EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT,
    };
    for (uint32_t plane = 0; plane < events->object_total; ++plane) {
        attributes[attribute_count++] = fd_attributes[plane];
        attributes[attribute_count++] = events->object_fds[plane];
        attributes[attribute_count++] = offset_attributes[plane];
        attributes[attribute_count++] = (EGLint)events->object_offsets[plane];
        attributes[attribute_count++] = pitch_attributes[plane];
        attributes[attribute_count++] = (EGLint)events->object_strides[plane];
        if (needs_modifier_attributes) {
            attributes[attribute_count++] = modifier_lo_attributes[plane];
            attributes[attribute_count++] = (EGLint)events->modifier_low;
            attributes[attribute_count++] = modifier_hi_attributes[plane];
            attributes[attribute_count++] = (EGLint)events->modifier_high;
        }
    }
    attributes[attribute_count++] = EGL_NONE;
    PFNEGLCREATEIMAGEKHRPROC create_image =
        (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    if (!create_image)
        goto cleanup;
    image = create_image(display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attributes);
    if (image == EGL_NO_IMAGE_KHR)
        goto cleanup;

    glGenTextures(1, &source_texture);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, source_texture);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    image_target_texture(GL_TEXTURE_EXTERNAL_OES, image);

    glGenTextures(1, &target_texture);
    glBindTexture(GL_TEXTURE_2D, target_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        goto cleanup;

    static const char vertex_source[] =
        "attribute vec2 position; varying vec2 uv;"
        "void main() { uv = position * 0.5 + 0.5; gl_Position = vec4(position, 0.0, 1.0); }";
    static const char fragment_source[] =
        "#extension GL_OES_EGL_image_external : require\n"
        "precision mediump float; varying vec2 uv; uniform samplerExternalOES image;"
        "void main() { gl_FragColor = texture2D(image, uv); }";
    vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vertex_shader || !fragment_shader)
        goto cleanup;
    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glBindAttribLocation(program, 0, "position");
    glLinkProgram(program);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
        goto cleanup;
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, source_texture);
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    static const GLfloat positions[] = { -1.f, -1.f, 1.f, -1.f, -1.f, 1.f, 1.f, 1.f };
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, positions);
    glEnableVertexAttribArray(0);
    glViewport(0, 0, 1, 1);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GLubyte pixel[4] = { 0 };
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    if (glGetError() != GL_NO_ERROR)
        goto cleanup;
    result = pixel[0] > 200 && pixel[1] < 60 && pixel[2] < 60 ? 1 : 0;

cleanup:
    if (program)
        glDeleteProgram(program);
    if (vertex_shader)
        glDeleteShader(vertex_shader);
    if (fragment_shader)
        glDeleteShader(fragment_shader);
    if (framebuffer)
        glDeleteFramebuffers(1, &framebuffer);
    if (target_texture)
        glDeleteTextures(1, &target_texture);
    if (source_texture)
        glDeleteTextures(1, &source_texture);
    if (image != EGL_NO_IMAGE_KHR) {
        PFNEGLDESTROYIMAGEKHRPROC destroy_image =
            (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
        if (destroy_image)
            destroy_image(display, image);
    }
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT)
            eglDestroyContext(display, context);
        if (surface != EGL_NO_SURFACE)
            eglDestroySurface(display, surface);
        eglTerminate(display);
    }
    return result;
}

static void close_exported_fds(struct export_events *events)
{
    for (uint32_t index = 0; index < 4; ++index) {
        if (events->object_fds[index] >= 0) {
            close(events->object_fds[index]);
            events->object_fds[index] = -1;
        }
    }
}
#endif

static void frame_handle_ready(void *data, struct zwlr_export_dmabuf_frame_v1 *frame,
                               uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
    (void)frame;
    (void)tv_sec_hi;
    (void)tv_sec_lo;
    struct export_events *events = data;
    if (events->frame_count != 1 || events->object_count != events->object_total
        || events->cancel_count != 0 || tv_nsec >= 1000000000u)
        events->ordering_valid = 0;
    events->tv_nsec = tv_nsec;
    events->ready_count++;
}

static void frame_handle_cancel(void *data, struct zwlr_export_dmabuf_frame_v1 *frame,
                                uint32_t reason)
{
    (void)frame;
    struct export_events *events = data;
    if (events->ready_count != 0)
        events->ordering_valid = 0;
    events->cancel_reason = reason;
    events->cancel_count++;
}

static const struct zwlr_export_dmabuf_frame_v1_listener frame_listener = {
    .frame = frame_handle_frame,
    .object = frame_handle_object,
    .ready = frame_handle_ready,
    .cancel = frame_handle_cancel,
};

static struct wl_output *bind_output(struct client_connection *connection)
{
    return client_bind(connection, "wl_output", &wl_output_interface, 1);
}

static int capture_next_output(struct client_connection *connection,
                               struct zwlr_export_dmabuf_manager_v1 *manager,
                               struct wl_output *output, struct export_events *events)
{
    struct zwlr_export_dmabuf_frame_v1 *frame =
        zwlr_export_dmabuf_manager_v1_capture_output(manager, 1, output);
    if (!frame)
        return 0;
    zwlr_export_dmabuf_frame_v1_add_listener(frame, &frame_listener, events);

    // capture_output asks wlroots to render one future output frame. The first
    // roundtrip only guarantees that the server installed the output-commit
    // listener; wait for its terminal ready/cancel event before destroying the
    // frame resource.
    int dispatched = wl_display_roundtrip(connection->display) >= 0;
    while (dispatched && !events->ready_count && !events->cancel_count)
        dispatched = wl_display_dispatch(connection->display) >= 0;
    zwlr_export_dmabuf_frame_v1_destroy(frame);
    return dispatched;
}

int protocol_test_run(const char *socket_name)
{
    struct client_connection connection;
    struct xdg_toplevel_client toplevel = { 0 };
    struct export_events events = {
        .ordering_valid = 1,
        .object_payload_valid = 1,
    };
#ifdef TREELAND_PROTOCOL_EXPECT_DMABUF_READY
    for (uint32_t index = 0; index < 4; ++index)
        events.object_fds[index] = -1;
#endif
    if (!client_connect(&connection, socket_name))
        return 1;

    struct zwlr_export_dmabuf_manager_v1 *manager = client_bind(
        &connection, "zwlr_export_dmabuf_manager_v1", &zwlr_export_dmabuf_manager_v1_interface, 1);
    struct wl_output *output = bind_output(&connection);
    const int mapped = manager && output
        && xdg_toplevel_client_create_with_solid_buffer(
            &connection, &toplevel, 1920, 1080, 0xffff0000u);
    const int captured = mapped && capture_next_output(&connection, manager, output, &events);

    if (output)
        wl_output_destroy(output);
    if (manager)
        zwlr_export_dmabuf_manager_v1_destroy(manager);
    xdg_toplevel_client_destroy(&toplevel);
    client_disconnect(&connection);

    if (!mapped || !captured) {
#ifdef TREELAND_PROTOCOL_EXPECT_DMABUF_READY
        close_exported_fds(&events);
#endif
        fprintf(stderr, "wlr-export-dmabuf: failed to map a real output surface or dispatch capture\n");
        return 1;
    }

#ifdef TREELAND_PROTOCOL_EXPECT_DMABUF_READY
    if (events.cancel_count) {
        close_exported_fds(&events);
        fprintf(stderr, "wlr-export-dmabuf: GPU runner has no exportable DMA-BUF output; skipped\n");
        return 77;
    }
    const uint32_t required_mask = events.object_total == 32 ? UINT32_MAX : (1u << events.object_total) - 1;
    if (events.frame_count != 1 || events.ready_count != 1 || events.cancel_count != 0
        || !events.ordering_valid || !events.object_payload_valid
        || events.object_mask != required_mask) {
        fprintf(stderr,
                "wlr-export-dmabuf: invalid ready sequence frame=%u object=%u/%u ready=%u cancel=%u "
                "ordering=%d payload=%d mask=%#x\n",
                events.frame_count, events.object_count, events.object_total, events.ready_count,
                events.cancel_count, events.ordering_valid, events.object_payload_valid, events.object_mask);
        close_exported_fds(&events);
        return 1;
    }
    const int readback = readback_exported_red(&events);
    close_exported_fds(&events);
    if (readback < 0) {
        fprintf(stderr, "wlr-export-dmabuf: GPU runner lacks EGL DMA-BUF import/readback; skipped\n");
        return 77;
    }
    if (!readback) {
        fprintf(stderr, "wlr-export-dmabuf: exported output center was not the mapped red surface\n");
        return 1;
    }
#else
    if (events.frame_count != 0 || events.object_count != 0 || events.ready_count != 0
        || events.cancel_count != 1
        || events.cancel_reason != ZWLR_EXPORT_DMABUF_FRAME_V1_CANCEL_REASON_TEMPORARY) {
        fprintf(stderr,
                "wlr-export-dmabuf: pixman expected one temporary cancel, got frame=%u object=%u ready=%u "
                "cancel=%u reason=%u\n",
                events.frame_count, events.object_count, events.ready_count, events.cancel_count,
                events.cancel_reason);
        return 1;
    }
#endif
    return 0;
}
