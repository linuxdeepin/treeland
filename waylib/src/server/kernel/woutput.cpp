// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <wscopedvalue.h>
#include "woutput.h"
#include "wbackend.h"
#include "wcursor.h"
#include "wseat.h"
#include "woutputlayout.h"
#include "wscoplistener.h"
#include "wtools.h"
#include "platformplugin/qwlrootscreen.h"
#include "private/wglobal_p.h"
#include "wayliblogging.h"

#include <wlr_all.h>

#include <QCoreApplication>
#include <QQuickWindow>
#include <QCursor>

#include <xf86drm.h>
#include <drm_fourcc.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WOutputPrivate : public WObjectPrivate
{
public:
    WOutputPrivate(WOutput *qq, wlr_output *handle)
        : WObjectPrivate(qq)
        , m_handle(handle)
    {
        Q_ASSERT(handle);
        handle->data = qq;
    }

    inline wlr_output *handle() const {
        return m_handle;
    }

    inline QSize size() const {
        Q_ASSERT(handle());
        return QSize(handle()->width, handle()->height);
    }

    inline WOutput::Transform orientation() const {
        return static_cast<WOutput::Transform>(handle()->transform);
    }

    W_DECLARE_PUBLIC(WOutput)
    bool forceSoftwareCursor = false;

    QWlrootsScreen *screen = nullptr;
    QQuickWindow *window = nullptr;

    WBackend *backend = nullptr;
    WOutputLayout *layout = nullptr;

private:
    // The backend owns this handle and destroys it after notifying the
    // wrapper. Keep the address stable through that callback.
    wlr_output *m_handle = nullptr;
};

WOutput::WOutput(wlr_output *handle, WBackend *backend)
    : QObject(nullptr)
    , WObject(*new WOutputPrivate(this, handle))
{
    W_D(WOutput);
    d->backend = backend;
    listeners()->add(&handle->events.commit, this,
                   [this] (wlr_output_event_commit *event) {
        if (event->state->committed & WLR_OUTPUT_STATE_SCALE) {
            Q_EMIT scaleChanged();
            Q_EMIT effectiveSizeChanged();
        }

        if (event->state->committed & WLR_OUTPUT_STATE_MODE) {
            Q_EMIT modeChanged();
            Q_EMIT transformedSizeChanged();
            Q_EMIT effectiveSizeChanged();
        }

        if (event->state->committed & WLR_OUTPUT_STATE_TRANSFORM) {
            Q_EMIT orientationChanged();
            Q_EMIT transformedSizeChanged();
            Q_EMIT effectiveSizeChanged();
        }

        if (event->state->committed & WLR_OUTPUT_STATE_BUFFER)
            Q_EMIT bufferCommitted();

        if (event->state->committed & WLR_OUTPUT_STATE_ENABLED)
            Q_EMIT enabledChanged();

        if (event->state->committed & WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED)
            Q_EMIT adaptiveSyncEnabledChanged();
    });
}

WOutput::~WOutput()
{
    teardown();
    W_D(WOutput);
    Q_EMIT beforeDestroy();
    // Clear the reverse fromHandle() mapping while the native handle is
    // still alive (owner teardown deletes this wrapper before the native
    // output is destroyed, or from its destroy callback).
    if (d->m_handle && d->m_handle->data == this)
        d->m_handle->data = nullptr;
    // Drop from the layout (owner teardown with a live handle included).
    if (d->layout)
        d->layout->remove(this);
}

WBackend *WOutput::backend() const
{
    W_DC(WOutput);
    return d->backend;
}

WServer *WOutput::server() const
{
    W_DC(WOutput);
    return d->backend->server();
}

wlr_renderer *WOutput::renderer() const
{
    W_DC(WOutput);
    return d->handle()->renderer;
}

wlr_swapchain *WOutput::swapchain() const
{
    W_DC(WOutput);
    return d->handle()->swapchain;
}

wlr_allocator *WOutput::allocator() const
{
    W_DC(WOutput);
    return d->handle()->allocator;
}

// Copy from wlroots
static const struct wlr_drm_format_set *wlr_renderer_get_render_formats(
    struct wlr_renderer *r) {
    if (!r->impl->get_render_formats) {
        return NULL;
    }
    return r->impl->get_render_formats(r);
}

static bool wlr_drm_format_copy(struct wlr_drm_format *dst, const struct wlr_drm_format *src) {
    assert(src->len <= src->capacity);

    uint64_t *modifiers = reinterpret_cast<uint64_t*>(malloc(sizeof(*modifiers) * src->len));
    if (!modifiers) {
        return false;
    }

    memcpy(modifiers, src->modifiers, sizeof(*modifiers) * src->len);

    wlr_drm_format_finish(dst);
    dst->capacity = src->len;
    dst->len = src->len;
    dst->format = src->format;
    dst->modifiers = modifiers;
    return true;
}

static bool wlr_drm_format_has(const struct wlr_drm_format *fmt, uint64_t modifier) {
    for (size_t i = 0; i < fmt->len; ++i) {
        if (fmt->modifiers[i] == modifier) {
            return true;
        }
    }
    return false;
}

static bool wlr_drm_format_add(struct wlr_drm_format *fmt, uint64_t modifier) {
    if (wlr_drm_format_has(fmt, modifier)) {
        return true;
    }

    if (fmt->len == fmt->capacity) {
        size_t capacity = fmt->capacity ? fmt->capacity * 2 : 4;

        uint64_t *new_modifiers = reinterpret_cast<uint64_t*>(realloc(fmt->modifiers, sizeof(*fmt->modifiers) * capacity));
        if (!new_modifiers) {
            qCCritical(lcWlOutputBuffer) << "Failed to allocate memory for DRM format modifiers";
            return false;
        }

        fmt->capacity = capacity;
        fmt->modifiers = new_modifiers;
    }

    fmt->modifiers[fmt->len++] = modifier;
    return true;
}

static bool wlr_drm_format_intersect(struct wlr_drm_format *dst,
                                     const struct wlr_drm_format *a, const struct wlr_drm_format *b) {
    assert(a->format == b->format);

    size_t capacity = a->len < b->len ? a->len : b->len;
    uint64_t *modifiers = reinterpret_cast<uint64_t*>(malloc(sizeof(*modifiers) * capacity));
    if (!modifiers) {
        return false;
    }

    wlr_drm_format fmt = {
        .format = a->format,
        .len = 0,
        .capacity = capacity,
        .modifiers = modifiers,
    };

    for (size_t i = 0; i < a->len; i++) {
        for (size_t j = 0; j < b->len; j++) {
            if (a->modifiers[i] == b->modifiers[j]) {
                assert(fmt.len < fmt.capacity);
                fmt.modifiers[fmt.len++] = a->modifiers[i];
                break;
            }
        }
    }

    wlr_drm_format_finish(dst);
    *dst = fmt;
    return true;
}

static bool output_pick_format(struct wlr_output *output,
                               const struct wlr_drm_format_set *display_formats,
                               struct wlr_drm_format *format, uint32_t fmt) {
    struct wlr_renderer *renderer = output->renderer;
    assert(renderer != NULL && output->allocator != NULL);

    const struct wlr_drm_format_set *render_formats =
        wlr_renderer_get_render_formats(renderer);
    if (render_formats == NULL) {
        qCCritical(lcWlOutputDrm) << "Failed to get renderer format support information";
        return false;
    }

    const struct wlr_drm_format *render_format =
        wlr_drm_format_set_get(render_formats, fmt);
    if (render_format == NULL) {
        qCDebug(lcWlOutputDrm) << "Renderer does not support format:" << QString("0x%1").arg(fmt, 0, 16);
        return false;
    }

    if (display_formats != NULL) {
        const struct wlr_drm_format *display_format =
            wlr_drm_format_set_get(display_formats, fmt);
        if (display_format == NULL) {
            qCDebug(lcWlOutputDrm) << "Output does not support format:" << QString("0x%1").arg(fmt, 0, 16);
            return false;
        }
        if (!wlr_drm_format_intersect(format, display_format, render_format)) {
            qCWarning(lcWlOutputDrm) << "Failed to find compatible format modifiers for format" 
                                     << QString("0x%1").arg(fmt, 0, 16) 
                                     << "on output:" << QString::fromUtf8(output->name);
            return false;
        }
    } else {
        // The output can display any format
        if (!wlr_drm_format_copy(format, render_format)) {
            return false;
        }
    }

    if (format->len == 0) {
        // The caller owns `format` through WDrmFormat RAII, which finishes
        // it on scope exit; an explicit finish here would double-free the
        // modifiers buffer.
        qCWarning(lcWlOutputDrm) << "No compatible output format found";
        return false;
    }

    return true;
}

static struct wlr_swapchain *create_swapchain(struct wlr_output *output,
                                              int width, int height,
                                              uint32_t render_format,
                                              bool allow_modifiers) {
    wlr_allocator *allocator = output->allocator;
    assert(output->allocator != NULL);

    const struct wlr_drm_format_set *display_formats =
        wlr_output_get_primary_formats(output, allocator->buffer_caps);
    WDrmFormat format;
    if (!output_pick_format(output, display_formats, format.get(), render_format)) {
        qCWarning(lcWlOutputBuffer) << "Failed to pick primary buffer format for output:" << QString::fromUtf8(output->name);
        return NULL;
    }

    char *format_name = drmGetFormatName(format->format);
    qCInfo(lcWlOutputBuffer) << "Selected primary buffer format:" 
                              << (format_name ? QString::fromUtf8(format_name) : QString("<unknown>"))
                              << QString("(0x%1)").arg(format->format, 8, 16, QLatin1Char('0'))
                              << "for output:" << QString::fromUtf8(output->name);
    free(format_name);

    if (!allow_modifiers && (format->len != 1 || format->modifiers[0] != DRM_FORMAT_MOD_LINEAR)) {
        if (!wlr_drm_format_has(format.get(), DRM_FORMAT_MOD_INVALID)) {
            qCWarning(lcWlOutputDrm) << "No support for implicit modifiers";
                        return NULL;
        }

        format->len = 0;
        if (!wlr_drm_format_add(format.get(), DRM_FORMAT_MOD_INVALID)) {
            qCWarning(lcWlOutputDrm) << "Failed to add implicit modifier to DRM format";
                        return NULL;
        }
    }

    struct wlr_swapchain *swapchain = wlr_swapchain_create(allocator, width, height, format.get());
        return swapchain;
}

static bool test_swapchain(struct wlr_output *output,
                           struct wlr_swapchain *swapchain, const struct wlr_output_state *state) {
    struct wlr_buffer *buffer = wlr_swapchain_acquire(swapchain);
    if (buffer == NULL) {
        return false;
    }

    struct wlr_output_state copy = *state;
    copy.committed |= WLR_OUTPUT_STATE_BUFFER;
    copy.buffer = buffer;
    bool ok = wlr_output_test_state(output, &copy);
    wlr_buffer_unlock(buffer);
    return ok;
}

static bool wlr_output_configure_primary_swapchain(struct wlr_output *output, int width, int height,
                                                   uint32_t format, struct wlr_swapchain **swapchain_ptr,
                                                   bool test) {
    WOutputStateGuard empty_state;
    wlr_output_state *state = empty_state.get();

    // Re-use the existing swapchain if possible
    struct wlr_swapchain *old_swapchain = *swapchain_ptr;
    if (old_swapchain != NULL &&
        old_swapchain->width == width && old_swapchain->height == height &&
        old_swapchain->format.format == format) {
        return true;
    }

    struct wlr_swapchain *swapchain = create_swapchain(output, width, height, format, true);
    if (swapchain == NULL) {
        qCCritical(lcWlOutputBuffer) << "Failed to create swapchain for output:" << QString::fromUtf8(output->name);
        return false;
    }

    if (test) {
        qCDebug(lcWlOutputBuffer) << "Testing swapchain for output:" << QString::fromUtf8(output->name);
        if (!test_swapchain(output, swapchain, state)) {
            qCDebug(lcWlOutputBuffer) << "Output test failed for" << QString::fromUtf8(output->name) 
                                      << "- retrying without modifiers";
            wlr_swapchain_destroy(swapchain);
            swapchain = create_swapchain(output, width, height, format, false);
            if (swapchain == NULL) {
                qCCritical(lcWlOutputBuffer) << "Failed to create modifier-less swapchain for output:" 
                                             << QString::fromUtf8(output->name);
                return false;
            }
            qCDebug(lcWlOutputBuffer) << "Testing modifier-less swapchain for output:" 
                                      << QString::fromUtf8(output->name);
            if (!test_swapchain(output, swapchain, state)) {
                qCCritical(lcWlOutputBuffer) << "Swapchain test failed for output:" 
                                             << QString::fromUtf8(output->name);
                wlr_swapchain_destroy(swapchain);
                return false;
            }
        }
    }

    wlr_swapchain_destroy(*swapchain_ptr);
    *swapchain_ptr = swapchain;
    return true;
}

static bool output_pick_cursor_format(struct wlr_output *output,
                                      struct wlr_drm_format *format,
                                      uint32_t drm_format) {
    struct wlr_allocator *allocator = output->allocator;
    assert(allocator != NULL);

    const struct wlr_drm_format_set *display_formats = NULL;
    if (output->impl->get_cursor_formats) {
        display_formats =
            output->impl->get_cursor_formats(output, allocator->buffer_caps);
        if (display_formats == NULL) {
            qCDebug(lcWlOutputDrm) << "Failed to get cursor display formats from output";
            return false;
        }
    }

    return output_pick_format(output, display_formats, format, drm_format);
}
// End

bool WOutput::configurePrimarySwapchain(const QSize &size, uint32_t format,
                                        wlr_swapchain **swapchain, bool doTest)
{
    W_D(WOutput);
    Q_ASSERT(!size.isEmpty());
    wlr_swapchain *sc = *swapchain;
    bool ok = wlr_output_configure_primary_swapchain(d->handle(), size.width(), size.height(),
                                                     format, &sc, doTest);
    if (!ok)
        return false;
    *swapchain = sc;
    return true;
}

bool WOutput::configureCursorSwapchain(const QSize &size, uint32_t drmFormat, wlr_swapchain **swapchain)
{
    W_D(WOutput);
    Q_ASSERT(!size.isEmpty());
    auto sc = *swapchain;
    if (!sc || sc->width != size.width() || sc->height != size.height()) {
        WDrmFormat format;
        if (!output_pick_cursor_format(d->handle(), format.get(), drmFormat)) {
            qCDebug(lcWlOutputDrm) << "Failed to select compatible cursor format";
            return false;
        }

        wlr_swapchain_destroy(sc);
        sc = wlr_swapchain_create(allocator(), size.width(), size.height(), format.get());
                if (!sc) {
            qCDebug(lcWlOutputBuffer) << "Failed to create cursor swapchain with selected format";
            return false;
        }
    }

    *swapchain = sc;
    return true;
}

wlr_output *WOutput::handle() const
{
    W_DC(WOutput);
    return d->handle();
}

WOutput *WOutput::fromHandle(wlr_output *handle)
{
    if (!handle)
        return nullptr;
    return static_cast<WOutput*>(handle->data);
}

WOutput *WOutput::fromScreen(const QScreen *screen)
{
    return static_cast<QWlrootsScreen*>(screen->handle())->output();
}

void WOutput::setScreen(QWlrootsScreen *screen)
{
    W_D(WOutput);
    d->screen = screen;
}

QWlrootsScreen *WOutput::screen() const
{
    W_DC(WOutput);
    return d->screen;
}

QString WOutput::name() const
{
    W_DC(WOutput);
    return QString::fromUtf8(d->handle()->name);
}

bool WOutput::isEnabled() const
{
    W_DC(WOutput);
    return d->handle()->enabled;
}

QPoint WOutput::position() const
{
    W_DC(WOutput);

    QPoint p;

    if (Q_UNLIKELY(!d->layout))
        return p;

    auto l_output = wlr_output_layout_get(d->layout->handle(), d->handle());

    if (Q_UNLIKELY(!l_output))
        return p;

    return QPoint(l_output->x, l_output->y);
}

QSize WOutput::size() const
{
    W_DC(WOutput);

    return d->size();
}

QSize WOutput::transformedSize() const
{
    W_DC(WOutput);
    int width, height;
    wlr_output_transformed_resolution(d->handle(), &width, &height);
    return QSize( width, height );
}

QSize WOutput::effectiveSize() const
{
    W_DC(WOutput);

    int width, height;
    wlr_output_effective_resolution(d->handle(), &width, &height);
    return QSize( width, height );
}

WOutput::Transform WOutput::orientation() const
{
    W_DC(WOutput);

    return d->orientation();
}

float WOutput::scale() const
{
    W_DC(WOutput);

    return d->handle()->scale;
}

void WOutput::attach(QQuickWindow *window)
{
    W_D(WOutput);
    d->window = window;
}

QQuickWindow *WOutput::attachedWindow() const
{
    W_DC(WOutput);
    return d->window;
}

void WOutput::setLayout(WOutputLayout *layout)
{
    W_D(WOutput);

    if (d->layout == layout)
        return;

    d->layout = layout;
}

WOutputLayout *WOutput::layout() const
{
    W_DC(WOutput);

    return d->layout;
}

void WOutput::addCursor(WCursor *cursor)
{
    static_cast<QWlrootsCursor*>(screen()->cursor())->addCursor(cursor);
    Q_EMIT cursorAdded(cursor);
    Q_EMIT cursorListChanged();
}

void WOutput::removeCursor(WCursor *cursor)
{
    static_cast<QWlrootsCursor*>(screen()->cursor())->removeCursor(cursor);
    Q_EMIT cursorRemoved(cursor);
    Q_EMIT cursorListChanged();
}

const QList<WCursor *> &WOutput::cursorList() const
{
    return static_cast<QWlrootsCursor*>(screen()->cursor())->cursors;
}

bool WOutput::forceSoftwareCursor() const
{
    W_DC(WOutput);
    return d->forceSoftwareCursor;
}

void WOutput::setForceSoftwareCursor(bool on)
{
    W_D(WOutput);
    if (d->forceSoftwareCursor == on)
        return;
    d->forceSoftwareCursor = on;
    wlr_output_lock_software_cursors(d->handle(), on);

    Q_EMIT forceSoftwareCursorChanged();
}

void WOutput::scheduleFrame()
{
    wlr_output_schedule_frame(handle());
}

WAYLIB_SERVER_END_NAMESPACE
