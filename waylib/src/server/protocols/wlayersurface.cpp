// Copyright (C) 2023 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wlayersurface.h"
#include "wseat.h"
#include "wsurface.h"
#include "woutput.h"
#include "wayliblogging.h"
#include "private/wtoplevelsurface_p.h"

#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_xdg_shell.h>

#include <QDebug>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WLayerSurfacePrivate : public WToplevelSurfacePrivate {
public:
    WLayerSurfacePrivate(WLayerSurface *qq, wlr_layer_surface_v1 *handle);
    ~WLayerSurfacePrivate();

    WWRAP_NATIVE_HANDLE_FUNCTIONS(wlr_layer_surface_v1)

    wl_client *waylandClient() const override {
        return handle()->resource->client;
    }

    // begin slot function
    void updateLayerProperty();
    // end slot function

    void init();
    void connect();
    void instantRelease() override;

    bool setDesiredSize(QSize newSize);
    bool setLayer(WLayerSurface::LayerType layer);
    bool setAncher(WLayerSurface::AnchorTypes ancher);
    bool setExclusiveZone(int32_t zone);
    bool setLeftMargin(int32_t margin);
    bool setRightMargin(int32_t margin);
    bool setTopMargin(int32_t margin);
    bool setBottomMargin(int32_t margin);
    bool setKeyboardInteractivity(WLayerSurface::KeyboardInteractivity interactivity);

    W_DECLARE_PUBLIC(WLayerSurface)

    WScopedListener m_surfaceCommitListener;
    WScopedListener m_newPopupListener;

    WSurface *surface = nullptr;
    QSize desiredSize;
    WLayerSurface::LayerType layer = WLayerSurface::LayerType::Bottom;
    WLayerSurface::AnchorTypes ancher =  WLayerSurface::AnchorType::None;
    int32_t leftMargin = 0, rightMargin = 0;
    int32_t topMargin = 0, bottomMargin = 0;
    int32_t exclusiveZone = 0;
    WLayerSurface::KeyboardInteractivity keyboardInteractivity = WLayerSurface::KeyboardInteractivity::None;
    WOutput *output = nullptr;
};

WLayerSurfacePrivate::WLayerSurfacePrivate(WLayerSurface *qq, wlr_layer_surface_v1 *hh)
    : WToplevelSurfacePrivate(qq)
{
    initNativeHandle(hh, &hh->events.destroy);
}

WLayerSurfacePrivate::~WLayerSurfacePrivate()
{

}

void WLayerSurfacePrivate::instantRelease()
{
    m_surfaceCommitListener.remove();
    m_newPopupListener.remove();

    if (!surface)
        return;
    surface->safeDeleteLater();
    surface = nullptr;
}

void WLayerSurfacePrivate::init()
{
    W_Q(WLayerSurface);

    Q_ASSERT(!q->surface());
    surface = new WSurface(handle()->surface, q);
    surface->setAttachedData<WLayerSurface>(q);
    updateLayerProperty();
    output = handle()->output ? WOutput::fromHandle(handle()->output) : nullptr;

    connect();
}

void WLayerSurfacePrivate::connect()
{
    W_Q(WLayerSurface);

    m_surfaceCommitListener.connect(&surface->handle()->events.commit, [this] (wl_listener *, void *) {
        updateLayerProperty();
    });
    m_newPopupListener.connect(&handle()->events.new_popup, [q] (wl_listener *, void *data) {
        Q_EMIT q->newPopup(static_cast<wlr_xdg_popup*>(data));
    });
}

[[maybe_unused]] static inline void debugOutput(wlr_layer_surface_v1_state s)
{
    qCDebug(lcWlLayerSurface) << "committed: " << s.committed << " "
             << "configure_serial: " << s.configure_serial << "\n"
             << "anchor: " << s.anchor << " "
             << "layer: " << s.layer << " "
             << "exclusive_zone: " << s.exclusive_zone << " "
             << "keyboard_interactive: " << s.keyboard_interactive << "\n"
             << "margin: " << s.margin.left << s.margin.right << s.margin.top << s.margin.bottom << "\n"
             << "desired_width/height: " << s.desired_width << " " << s.desired_height << "\n"
             << "actual_width/height" << s.actual_width << " " << s.actual_height << "\n";
}

void WLayerSurfacePrivate::updateLayerProperty()
{
    W_Q(WLayerSurface);

    wlr_layer_surface_v1_state state = q->handle()->current;

    int hasChange = false;
    hasChange |= setDesiredSize(QSize(state.desired_width, state.desired_height));
    hasChange |= setLayer(static_cast<WLayerSurface::LayerType>(state.layer));
    hasChange |= setAncher(WLayerSurface::AnchorTypes::fromInt(state.anchor));
    hasChange |= setExclusiveZone(state.exclusive_zone);
    hasChange |= setLeftMargin(state.margin.left);
    hasChange |= setRightMargin(state.margin.right);
    hasChange |= setTopMargin(state.margin.top);
    hasChange |= setBottomMargin(state.margin.bottom);
    hasChange |= setKeyboardInteractivity(static_cast<WLayerSurface::KeyboardInteractivity>(state.keyboard_interactive));

    if (hasChange)
        Q_EMIT q->layerPropertiesChanged();

}

// begin set property

bool WLayerSurfacePrivate::setDesiredSize(QSize newSize)
{
    W_Q(WLayerSurface);
    if (desiredSize == newSize)
        return false;
    desiredSize = newSize;
    Q_EMIT q->desiredSizeChanged();
    return true;
}


bool WLayerSurfacePrivate::setLayer(WLayerSurface::LayerType layer)
{
    W_Q(WLayerSurface);
    if (this->layer == layer)
        return false;
    this->layer = layer;
    Q_EMIT q->layerChanged();
    return true;
}

bool WLayerSurfacePrivate::setAncher(WLayerSurface::AnchorTypes ancher)
{
    W_Q(WLayerSurface);
    if (this->ancher == ancher)
        return false;
    this->ancher = ancher;
    Q_EMIT q->ancherChanged();
    return true;
}

bool WLayerSurfacePrivate::setExclusiveZone(int32_t zone)
{
    W_Q(WLayerSurface);
    if (exclusiveZone == zone)
        return false;
    exclusiveZone = zone;
    Q_EMIT q->exclusiveZoneChanged();
    return true;
}

bool WLayerSurfacePrivate::setLeftMargin(int32_t margin)
{
    W_Q(WLayerSurface);
    if (leftMargin == margin)
        return false;
    leftMargin = margin;
    Q_EMIT q->leftMarginChanged();
    return true;
}

bool WLayerSurfacePrivate::setRightMargin(int32_t margin)
{
    W_Q(WLayerSurface);
    if (rightMargin == margin)
        return false;
    rightMargin = margin;
    Q_EMIT q->rightMarginChanged();
    return true;
}

bool WLayerSurfacePrivate::setTopMargin(int32_t margin)
{
    W_Q(WLayerSurface);
    if (topMargin == margin)
        return false;
    topMargin = margin;
    Q_EMIT q->topMarginChanged();
    return true;
}

bool WLayerSurfacePrivate::setBottomMargin(int32_t margin)
{
    W_Q(WLayerSurface);
    if (bottomMargin == margin)
        return false;
    bottomMargin = margin;
    Q_EMIT q->bottomMarginChanged();
    return true;
}

bool WLayerSurfacePrivate::setKeyboardInteractivity(WLayerSurface::KeyboardInteractivity interactivity)
{
    W_Q(WLayerSurface);
    if (keyboardInteractivity == interactivity)
        return false;
    keyboardInteractivity = interactivity;
    Q_EMIT q->keyboardInteractivityChanged();
    return true;
}

// end set property

WLayerSurface::WLayerSurface(wlr_layer_surface_v1 *handle, QObject *parent)
    : WToplevelSurface(*new WLayerSurfacePrivate(this, handle), parent)
{
    d_func()->init();
}

WLayerSurface::~WLayerSurface()
{

}

bool WLayerSurface::hasCapability(Capability cap) const
{
    W_DC(WLayerSurface);
    switch (cap) {
        using enum Capability;
    case Focus:
        return d->keyboardInteractivity != WLayerSurface::KeyboardInteractivity::None;
    case Activate:
    case Maximized:
    case FullScreen:
        return false;
    case Resize:
        return true;
    default:
        break;
    }
    Q_UNREACHABLE();
}

WSurface *WLayerSurface::surface() const
{
    W_D(const WLayerSurface);
    return d->surface;
}

wlr_layer_surface_v1 *WLayerSurface::handle() const
{
    W_DC(WLayerSurface);
    return d->handle();
}

wlr_surface *WLayerSurface::inputTargetAt(QPointF &localPos) const
{
    W_DC(WLayerSurface);
    // find a wlr_surface object who can receive the events
    const QPointF pos = localPos;
    return wlr_layer_surface_v1_surface_at(d->handle(), pos.x(), pos.y(), &localPos.rx(), &localPos.ry());
}

WLayerSurface *WLayerSurface::fromHandle(wlr_layer_surface_v1 *handle)
{
    return static_cast<WLayerSurface*>(WWrapObjectPrivate::fromNativeHandle(handle));
}

WLayerSurface *WLayerSurface::fromSurface(WSurface *surface)
{
    return surface->getAttachedData<WLayerSurface>();
}

QRect WLayerSurface::getContentGeometry() const
{
    W_DC(WLayerSurface);
    return QRect(0, 0, d->handle()->current.actual_width, d->handle()->current.actual_height);
}

int WLayerSurface::keyboardFocusPriority() const
{
    W_DC(WLayerSurface);

    if (d->keyboardInteractivity == KeyboardInteractivity::None)
        return -1;

    if (d->keyboardInteractivity == KeyboardInteractivity::Exclusive) {
        if (d->layer == LayerType::Overlay)
            return 2;
        if (d->layer == LayerType::Top)
            return 1;
        // For the bottom and background layers
        // the compositor is allowed to use normal focus semantics
    }

    return 0;
}

bool WLayerSurface::isInitialized() const
{
    return d->handle()->initialized;
}

void WLayerSurface::resize(const QSize &size)
{
    configureSize(size);
}

void WLayerSurface::close()
{
    closed();
}

QSize WLayerSurface::desiredSize() const
{
    W_DC(WLayerSurface);
    return d->desiredSize;
}

WLayerSurface::LayerType WLayerSurface::layer() const
{
    W_DC(WLayerSurface);
    return d->layer;
}

WLayerSurface::AnchorTypes WLayerSurface::ancher() const
{
    W_DC(WLayerSurface);
    return d->ancher;
}

int32_t WLayerSurface::leftMargin() const
{
    W_DC(WLayerSurface);
    return d->leftMargin;
}

int32_t WLayerSurface::rightMargin() const
{
    W_DC(WLayerSurface);
    return d->rightMargin;
}

int32_t WLayerSurface::topMargin() const
{
    W_DC(WLayerSurface);
    return d->topMargin;
}

int32_t WLayerSurface::bottomMargin() const
{
    W_DC(WLayerSurface);
    return d->bottomMargin;
}

int32_t WLayerSurface::exclusiveZone() const
{
    W_DC(WLayerSurface);
    return d->exclusiveZone;
}

WLayerSurface::KeyboardInteractivity WLayerSurface::keyboardInteractivity() const
{
    W_DC(WLayerSurface);
    return d->keyboardInteractivity;
}

QString WLayerSurface::scope() const
{
    return QString::fromLocal8Bit(d_func()->handle()->scope);
}

WOutput *WLayerSurface::output() const
{
    W_DC(WLayerSurface);
    return d->output;
}

WLayerSurface::AnchorType WLayerSurface::getExclusiveZoneEdge() const
{
    W_DC(WLayerSurface);
    using enum WLayerSurface::AnchorType;

    auto ancher = d->ancher;
    if (ancher == Top || ancher == (Top|Left|Right))
        return Top;
    if (ancher == Bottom || ancher == (Bottom|Left|Right))
        return Bottom;
    if (ancher == Left || ancher == (Left|Top|Bottom))
        return Left;
    if (ancher == Right || ancher == (Right|Top|Bottom))
        return Right;
    return None;
}

uint32_t WLayerSurface::configureSize(const QSize &newSize)
{
    return wlr_layer_surface_v1_configure(d_func()->handle(), newSize.width(), newSize.height());
}

void WLayerSurface::closed()
{
    // 1. Notify the client that the surface has been closed
    // 2. Destroy the struct wlr_layer_surface_v1
    wlr_layer_surface_v1_destroy(d_func()->handle());
}

bool WLayerSurface::checkNewSize(const QSize &size,  QSize *clipedSize)
{
    // If the width or height arguments are zero, it means the client should decide its own window dimension.
    if (size.width() < 0 || size.height() < 0) {
        if (clipedSize)
            *clipedSize = QSize(0, 0);
        return false;
    }
    return true;
}

WAYLIB_SERVER_END_NAMESPACE
