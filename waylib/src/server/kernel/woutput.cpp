// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "woutput.h"
#include "wbackend.h"
#include "wcursor.h"
#include "wseat.h"
#include "wtools.h"
#include "platformplugin/qwlrootscreen.h"
#include "private/wglobal_p.h"

#include <qwoutput.h>
#include <qwoutputlayout.h>
#include <qwrenderer.h>
#include <qwswapchain.h>
#include <qwallocator.h>
#include <qwrendererinterface.h>
#include <qwoutputinterface.h>

#include <QLoggingCategory>
#include <QCoreApplication>
#include <QQuickWindow>
#include <QCursor>

#include <xf86drm.h>
#include <drm_fourcc.h>

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qLcOutput, "waylib.server.output", QtWarningMsg)

class Q_DECL_HIDDEN WOutputPrivate : public WWrapObjectPrivate
{
public:
    WOutputPrivate(WOutput *qq, qw_output *handle)
        : WWrapObjectPrivate(qq)
    {
        initHandle(handle);
        this->handle()->set_data(this, qq);
    }

    void instantRelease() override {
        handle()->set_data(nullptr, nullptr);
        if (layout)
            layout->remove(q_func());
    }

    WWRAP_HANDLE_FUNCTIONS(qw_output, wlr_output)

    inline QSize size() const {
        Q_ASSERT(handle());
        return QSize(nativeHandle()->width, nativeHandle()->height);
    }

    inline WOutput::Transform orientation() const {
        return static_cast<WOutput::Transform>(nativeHandle()->transform);
    }

    W_DECLARE_PUBLIC(WOutput)

    bool forceSoftwareCursor = false;
    QWlrootsScreen *screen = nullptr;
    QQuickWindow *window = nullptr;

    WBackend *backend = nullptr;
    WOutputLayout *layout = nullptr;
};

WOutput::WOutput(qw_output *handle, WBackend *backend)
    : WWrapObject(*new WOutputPrivate(this, handle))
{
    d_func()->backend = backend;
    connect(handle, qOverload<wlr_output_event_commit*>(&qw_output::notify_commit),
            this, [this] (wlr_output_event_commit *event) {
        if (event->state->committed & WLR_OUTPUT_STATE_SCALE) {
            Q_EMIT this->scaleChanged();
            Q_EMIT this->effectiveSizeChanged();
        }

        if (event->state->committed & WLR_OUTPUT_STATE_MODE) {
            Q_EMIT this->modeChanged();
            Q_EMIT this->transformedSizeChanged();
            Q_EMIT this->effectiveSizeChanged();
        }

        if (event->state->committed & WLR_OUTPUT_STATE_TRANSFORM) {
            Q_EMIT this->orientationChanged();
            Q_EMIT this->transformedSizeChanged();
            Q_EMIT this->effectiveSizeChanged();
        }

        if (event->state->committed & WLR_OUTPUT_STATE_BUFFER)
            Q_EMIT this->bufferCommitted();

        if (event->state->committed & WLR_OUTPUT_STATE_ENABLED)
            Q_EMIT this->enabledChanged();
    });
}

WOutput::~WOutput()
{

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

qw_renderer *WOutput::renderer() const
{
    W_DC(WOutput);
    return qw_renderer::from(d->nativeHandle()->renderer);
}

qw_swapchain *WOutput::swapchain() const
{
    W_DC(WOutput);
    return qw_swapchain::from(d->nativeHandle()->swapchain);
}

qw_allocator *WOutput::allocator() const
{
    W_DC(WOutput);
    return qw_allocator::from(d->nativeHandle()->allocator);
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
            qCritical("Allocation failed");
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
    struct wlr_allocator *allocator = output->allocator;
    assert(renderer != NULL && allocator != NULL);

    const struct wlr_drm_format_set *render_formats =
        wlr_renderer_get_render_formats(renderer);
    if (render_formats == NULL) {
        qCritical("Failed to get render formats");
        return false;
    }

    const struct wlr_drm_format *render_format =
        wlr_drm_format_set_get(render_formats, fmt);
    if (render_format == NULL) {
        qDebug("Renderer doesn't support format 0x%" PRIX32, fmt);
        return false;
    }

    if (display_formats != NULL) {
        const struct wlr_drm_format *display_format =
            wlr_drm_format_set_get(display_formats, fmt);
        if (display_format == NULL) {
            qDebug("Output doesn't support format 0x%" PRIX32, fmt);
            return false;
        }
        if (!wlr_drm_format_intersect(format, display_format, render_format)) {
            qDebug("Failed to intersect display and render "
                   "modifiers for format 0x%" PRIX32 " on output %s",
                   fmt, output->name);
            return false;
        }
    } else {
        // The output can display any format
        if (!wlr_drm_format_copy(format, render_format)) {
            return false;
        }
    }

    if (format->len == 0) {
        wlr_drm_format_finish(format);
        qDebug("Failed to pick output format");
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
    struct wlr_drm_format format = {0};
    if (!output_pick_format(output, display_formats, &format, render_format)) {
        qDebug("Failed to pick primary buffer format for output '%s'",
               output->name);
        return NULL;
    }

    char *format_name = drmGetFormatName(format.format);
    qDebug("Choosing primary buffer format %s (0x%08" PRIX32 ") for output '%s'",
           format_name ? format_name : "<unknown>", format.format, output->name);
    free(format_name);

    if (!allow_modifiers && (format.len != 1 || format.modifiers[0] != DRM_FORMAT_MOD_LINEAR)) {
        if (!wlr_drm_format_has(&format, DRM_FORMAT_MOD_INVALID)) {
            qDebug("Implicit modifiers not supported");
            wlr_drm_format_finish(&format);
            return NULL;
        }

        format.len = 0;
        if (!wlr_drm_format_add(&format, DRM_FORMAT_MOD_INVALID)) {
            qDebug("Failed to add implicit modifier to format");
            wlr_drm_format_finish(&format);
            return NULL;
        }
    }

    struct wlr_swapchain *swapchain = wlr_swapchain_create(allocator, width, height, &format);
    wlr_drm_format_finish(&format);
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
    wlr_output_state empty_state;
    wlr_output_state_init(&empty_state);
    wlr_output_state *state = &empty_state;

    // Re-use the existing swapchain if possible
    struct wlr_swapchain *old_swapchain = *swapchain_ptr;
    if (old_swapchain != NULL &&
        old_swapchain->width == width && old_swapchain->height == height &&
        old_swapchain->format.format == format) {
        return true;
    }

    struct wlr_swapchain *swapchain = create_swapchain(output, width, height, format, true);
    if (swapchain == NULL) {
        qCritical("Failed to create swapchain for output '%s'", output->name);
        return false;
    }

    if (test) {
        qDebug("Testing swapchain for output '%s'", output->name);
        if (!test_swapchain(output, swapchain, state)) {
            qDebug("Output test failed on '%s', retrying without modifiers",
                   output->name);
            wlr_swapchain_destroy(swapchain);
            swapchain = create_swapchain(output, width, height, format, false);
            if (swapchain == NULL) {
                qCritical("Failed to create modifier-less swapchain for output '%s'",
                          output->name);
                return false;
            }
            qDebug("Testing modifier-less swapchain for output '%s'", output->name);
            if (!test_swapchain(output, swapchain, state)) {
                qCritical("Swapchain for output '%s' failed test", output->name);
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
            qCDebug(qLcOutput, "Failed to get cursor display formats");
            return false;
        }
    }

    return output_pick_format(output, display_formats, format, drm_format);
}
// End

bool WOutput::configurePrimarySwapchain(const QSize &size, uint32_t format,
                                        qw_swapchain **swapchain, bool doTest)
{
    Q_ASSERT(!size.isEmpty());
    wlr_swapchain *sc = (*swapchain)->handle();
    bool ok = wlr_output_configure_primary_swapchain(nativeHandle(), size.width(), size.height(),
                                                     format, &sc, doTest);
    if (!ok)
        return false;
    *swapchain = qw_swapchain::from(sc);
    return true;
}

bool WOutput::configureCursorSwapchain(const QSize &size, uint32_t drmFormat, qw_swapchain **swapchain)
{
    Q_ASSERT(!size.isEmpty());
    auto sc = *swapchain;
    if (!sc || sc->handle()->width != size.width() || sc->handle()->height != size.height()) {
        wlr_drm_format format = {0};
        if (!output_pick_cursor_format(nativeHandle(), &format, drmFormat)) {
            qCDebug(qLcOutput, "Failed to pick cursor format");
            return false;
        }

        delete sc;
        sc = qw_swapchain::create(*allocator(), size.width(), size.height(), &format);
        wlr_drm_format_finish(&format);
        if (!sc) {
            qCDebug(qLcOutput, "Failed to create cursor swapchain");
            return false;
        }
    }

    *swapchain = sc;
    return true;
}

qw_output *WOutput::handle() const
{
    W_DC(WOutput);
    return d->handle();
}

wlr_output *WOutput::nativeHandle() const
{
    W_DC(WOutput);
    return d->nativeHandle();
}

WOutput *WOutput::fromHandle(const qw_output *handle)
{
    return handle->get_data<WOutput>();
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
    return QString::fromUtf8(d->nativeHandle()->name);
}

bool WOutput::isEnabled() const
{
    W_DC(WOutput);
    return d->nativeHandle()->enabled;
}

QPoint WOutput::position() const
{
    W_DC(WOutput);

    QPoint p;

    if (Q_UNLIKELY(!d->layout))
        return p;

    auto l_output = d->layout->handle()->get(d->nativeHandle());

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
    d->handle()->transformed_resolution(&width, &height);
    return QSize( width, height );
}

QSize WOutput::effectiveSize() const
{
    W_DC(WOutput);

    int width, height;
    d->handle()->effective_resolution(&width, &height);
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

    return d->nativeHandle()->scale;
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
    d->handle()->lock_software_cursors(on);

    Q_EMIT forceSoftwareCursorChanged();
}

WAYLIB_SERVER_END_NAMESPACE
