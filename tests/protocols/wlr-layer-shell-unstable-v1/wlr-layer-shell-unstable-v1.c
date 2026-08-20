// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#define _POSIX_C_SOURCE 200809L
#include "client-connection.h"
#include "server-bridge-api.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "wlr-layer-shell-unstable-v1.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
extern void layer_shell_read_state(void *data);
struct events { uint32_t serial, width, height; unsigned int configure, closed; };
static void configure(void *d, struct zwlr_layer_surface_v1 *s, uint32_t serial, uint32_t width, uint32_t height) { (void)s; struct events *e=d; e->serial=serial; e->width=width; e->height=height; e->configure++; }
static void closed(void *d, struct zwlr_layer_surface_v1 *s) { (void)s; ((struct events*)d)->closed++; }
static const struct zwlr_layer_surface_v1_listener listener = { .configure=configure, .closed=closed };
static int make_buffer(struct wl_shm *shm, struct wl_buffer **buffer) {
    char name[]="/treeland-layer-XXXXXX"; int fd=shm_open(name,O_RDWR|O_CREAT|O_EXCL,0600); if(fd<0)return 0; shm_unlink(name); const int w=1920,h=40,stride=w*4,size=stride*h; if(ftruncate(fd,size)<0){close(fd);return 0;} void *p=mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0); if(p==MAP_FAILED){close(fd);return 0;} memset(p,0xff,size); struct wl_shm_pool *pool=wl_shm_create_pool(shm,fd,size); *buffer=pool?wl_shm_pool_create_buffer(pool,0,w,h,stride,WL_SHM_FORMAT_ARGB8888):NULL; if(pool)wl_shm_pool_destroy(pool); munmap(p,size); close(fd); return *buffer!=NULL;
}
int protocol_test_run(const char *socket) {
    struct client_connection c; struct wl_surface *surface=NULL; struct wl_buffer *buffer=NULL; unsigned int stage = 0; if(!client_connect(&c,socket))return 1;
    struct wl_compositor *compositor=client_bind(&c,"wl_compositor",&wl_compositor_interface,1); struct wl_shm *shm=client_bind(&c,"wl_shm",&wl_shm_interface,1); struct wl_output *output=client_bind(&c,"wl_output",&wl_output_interface,1); struct zwlr_layer_shell_v1 *shell=client_bind(&c,"zwlr_layer_shell_v1",&zwlr_layer_shell_v1_interface,5);
    if (!compositor || !shm || !output || !shell)
        goto fail;

    stage = 1;
    surface = wl_compositor_create_surface(compositor);
    if (!surface)
        goto fail;
    struct zwlr_layer_surface_v1 *layer=zwlr_layer_shell_v1_get_layer_surface(shell,surface,output,ZWLR_LAYER_SHELL_V1_LAYER_TOP,"protocol-test"); struct events e={0}; if(!layer)goto fail; zwlr_layer_surface_v1_add_listener(layer,&listener,&e);
    zwlr_layer_surface_v1_set_size(layer,1920,40); zwlr_layer_surface_v1_set_anchor(layer,ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP|ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT|ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT); zwlr_layer_surface_v1_set_exclusive_zone(layer,40); zwlr_layer_surface_v1_set_margin(layer,2,0,0,0); zwlr_layer_surface_v1_set_keyboard_interactivity(layer,ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE); zwlr_layer_surface_v1_set_exclusive_edge(layer,ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP); wl_surface_commit(surface);
    // A layer arrange can legitimately issue a replacement configure before
    // the client processes the first one. Acknowledge the newest serial.
    stage = 2; if(wl_display_roundtrip(c.display)<0||!e.configure||!e.serial)goto fail_layer; zwlr_layer_surface_v1_ack_configure(layer,e.serial); if(!make_buffer(shm,&buffer))goto fail_layer; wl_surface_attach(surface,buffer,0,0); wl_surface_damage(surface,0,0,1920,40); wl_surface_commit(surface); stage = 3; if(wl_display_roundtrip(c.display)<0)goto fail_layer;
    struct layer_shell_server_state state={0}; stage = 4; if(!invoke_on_server_thread(layer_shell_read_state,&state)||!state.wrapper||!state.container||state.layer!=ZWLR_LAYER_SHELL_V1_LAYER_TOP||state.anchor!=(ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP|ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT|ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)||state.exclusive_zone!=40||state.top_margin!=2||!state.keyboard_exclusive||!state.focused)goto fail_layer;
    zwlr_layer_surface_v1_set_layer(layer,ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY); wl_surface_commit(surface); stage = 5; if(wl_display_roundtrip(c.display)<0||!invoke_on_server_thread(layer_shell_read_state,&state)||state.layer!=ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY)goto fail_layer;
    zwlr_layer_surface_v1_destroy(layer); zwlr_layer_shell_v1_destroy(shell); wl_buffer_destroy(buffer); wl_surface_destroy(surface); wl_output_destroy(output); wl_shm_destroy(shm); wl_compositor_destroy(compositor); client_disconnect(&c); return 0;
fail_layer: fprintf(stderr, "wlr-layer-shell failure at stage %u: configure=%u serial=%u size=%ux%u state=(wrapper=%d container=%d layer=%d anchor=%d exclusive-zone=%d top-margin=%d keyboard-exclusive=%d focused=%d)\n", stage, e.configure, e.serial, e.width, e.height, state.wrapper, state.container, state.layer, state.anchor, state.exclusive_zone, state.top_margin, state.keyboard_exclusive, state.focused); zwlr_layer_surface_v1_destroy(layer); if(buffer)wl_buffer_destroy(buffer); if(surface)wl_surface_destroy(surface); fail: if(stage < 2) fprintf(stderr, "wlr-layer-shell failure at stage %u: missing required global or wl_surface\n", stage); if(shell)zwlr_layer_shell_v1_destroy(shell); if(output)wl_output_destroy(output); if(shm)wl_shm_destroy(shm); if(compositor)wl_compositor_destroy(compositor); client_disconnect(&c); return 1;
}
