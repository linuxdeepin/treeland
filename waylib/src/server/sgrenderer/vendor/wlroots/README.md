# Vendored wlroots 0.19 Renderer

This directory contains a vendored copy of the wlroots 0.19.3 renderer
implementation, with extensions that expose internal APIs for use by the
waylib sgrenderer Backend Abstraction Layer (BAL).

## Source

- **Version**: wlroots 0.19.3
- **Commit**: 88a869855742281c98c22cab9641b317b8d065ef
- **License**: MIT (preserved from upstream)

## Directory Structure

```
vendor/wlroots/
├── CMakeLists.txt          — Build target: waylib_sgrenderer_wlroots_vendor
├── config.h.in             — CMake-configured config.h template
├── include/render/         — Vendored internal headers (from include/render/)
│   ├── egl.h               — EGL context management
│   ├── gles2.h             — GLES2 renderer/texture/buffer structs
│   ├── vulkan.h            — Vulkan renderer/texture/device structs
│   ├── wlr_pixman.h        — Pixman renderer/texture structs
│   ├── dmabuf.h            — dmabuf attribute helpers
│   ├── drm_format_set.h    — DRM format set operations
│   ├── pixel_format.h      — Pixel format info
│   ├── wlr_renderer.h      — Internal renderer dispatch
│   ├── color.h             — Color transform internals
│   ├── allocator/          — Allocator implementations
│   ├── types/              — Internal type headers (wlr_buffer.h)
│   └── util/               — Utility headers (time, env, shm, matrix, rect_union)
├── render/                 — Vendored source files (from render/)
│   ├── egl.c               — EGL context + dmabuf→EGLImage
│   ├── dmabuf.c            — dmabuf attribute management
│   ├── swapchain.c         — Buffer swapchain
│   ├── drm_format_set.c    — DRM format set operations
│   ├── drm_syncobj.c       — DRM sync object (timeline semaphore)
│   ├── pixel_format.c      — Pixel format table
│   ├── pass.c              — Generic render pass
│   ├── wlr_renderer.c      — Renderer dispatch (auto_init, begin_buffer_pass)
│   ├── wlr_texture.c       — Texture dispatch
│   ├── color.c             — Color transform (no lcms2)
│   ├── gles2/              — GLES2 backend (renderer, texture, pass, pixel_format)
│   ├── vulkan/             — Vulkan backend (renderer, texture, pass, vulkan, util, pixel_format)
│   ├── pixman/             — Pixman backend (renderer, pass, pixel_format)
│   ├── allocator/          — Buffer allocators (allocator, gbm, drm_dumb, shm, udmabuf)
│   └── util/               — Utility implementations (matrix, time, env, shm, rect_union)
└── extensions/             — Extension headers + implementations
    ├── wsg_wlroots_gles2.h/c  — Expose gles2 buffer FBO, texture GL handle, EGL context
    ├── wsg_wlroots_vulkan.h/c — Expose vulkan texture VkImage, command buffer, device/queue
    ├── wsg_wlroots_pixman.h/c — Expose pixman buffer/texture pixman_image_t
    └── wsg_wlroots_egl.h/c    — Expose EGL make_current/restore, dmabuf→EGLImage
```

## Extensions

The extension headers in `extensions/` provide thin C wrappers that expose
internal struct fields (FBO, GL texture ID, VkImage, EGLImage, pixman_image_t,
command buffers) for direct use by the BAL layer, without going through
`wlr_render_pass`. Each function is a simple field accessor or function
delegation.

## Build

The CMake target `waylib_sgrenderer_wlroots_vendor` builds a static library.
Backend inclusion is controlled by CMake options:

- `WSG_VENDOR_ENABLE_GLES2` (default ON)
- `WSG_VENDOR_ENABLE_VULKAN` (default ON)
- `WSG_VENDOR_ENABLE_PIXMAN` (default ON)

The vulkan shader SPIR-V headers are generated at build time via
`glslangValidator` (required when the vulkan backend is enabled). The gles2 shader source headers are
pre-generated as C string arrays.

## Upgrade Notes

When upgrading from a newer wlroots version:
1. Replace files under `include/render/` and `render/`
2. Update the commit hash in each file's vendor header
3. Re-generate gles2 shader headers (run the embed approach)
4. Check for new struct fields needed by the extension accessors
5. Update `config.h.in` if new `HAVE_*` macros are introduced
