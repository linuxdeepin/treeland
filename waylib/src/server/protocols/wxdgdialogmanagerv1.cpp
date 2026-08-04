// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgdialogmanagerv1.h"

#include "private/wglobal_p.h"
#include "wayliblogging.h"
#include "wxdgtoplevelsurface.h"

#include <QPointer>

#include <memory>
#include <unordered_map>

extern "C" {
#include <wlr/types/wlr_xdg_dialog_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgDialogManagerV1Private : public WObjectPrivate
{
public:
    struct DialogListeners {
        WNativeListener setModal;
        WNativeListener destroy;
    };

    WXdgDialogManagerV1Private(WXdgDialogManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    void onNewDialog(wlr_xdg_dialog_v1 *dialog);

    W_DECLARE_PUBLIC(WXdgDialogManagerV1)

    WNativeListener newDialogListener;
    std::unordered_map<wlr_xdg_dialog_v1 *, std::unique_ptr<DialogListeners>> dialogListeners;
};

void WXdgDialogManagerV1Private::onNewDialog(wlr_xdg_dialog_v1 *dialog)
{
    W_Q(WXdgDialogManagerV1);

    auto *surface = WXdgToplevelSurface::fromHandle(dialog->xdg_toplevel);
    if (!surface) {
        qCWarning(lcWlXdgDialog)
            << "Ignoring xdg dialog without a matching Waylib toplevel" << dialog->xdg_toplevel;
        return;
    }

    qCDebug(lcWlXdgDialog) << "New xdg_dialog_v1 for surface:" << surface
                           << "initial modal:" << dialog->modal;

    QPointer<WXdgToplevelSurface> surfaceGuard(surface);
    if (dialog->modal)
        Q_EMIT q->surfaceModalChanged(surface, true);

    auto listeners = std::make_unique<DialogListeners>();
    listeners->setModal.connect(&dialog->events.set_modal,
                                [q, dialog, surfaceGuard](void *) {
        if (!surfaceGuard)
            return;
        qCDebug(lcWlXdgDialog) << "xdg_dialog_v1 modal changed:" << surfaceGuard
                               << "->" << dialog->modal;
        Q_EMIT q->surfaceModalChanged(surfaceGuard, dialog->modal);
    });
    listeners->destroy.connect(&dialog->events.destroy,
                               [this, q, dialog, surfaceGuard](void *) {
        if (dialog->modal && surfaceGuard) {
            qCDebug(lcWlXdgDialog) << "xdg_dialog_v1 destroyed while modal, resetting:"
                                   << surfaceGuard;
            Q_EMIT q->surfaceModalChanged(surfaceGuard, false);
        }
        dialogListeners.erase(dialog);
    });
    dialogListeners.emplace(dialog, std::move(listeners));
}

WXdgDialogManagerV1::WXdgDialogManagerV1(QObject *parent)
    : QObject(parent)
    , WObject(*new WXdgDialogManagerV1Private(this))
{
}

WXdgDialogManagerV1::~WXdgDialogManagerV1() = default;

wlr_xdg_wm_dialog_v1 *WXdgDialogManagerV1::handle() const
{
    return nativeInterface<wlr_xdg_wm_dialog_v1>();
}

QByteArrayView WXdgDialogManagerV1::interfaceName() const
{
    return "xdg_wm_dialog_v1";
}

void WXdgDialogManagerV1::create(WServer *server)
{
    W_D(WXdgDialogManagerV1);
    auto *manager = wlr_xdg_wm_dialog_v1_create(server->handle(), 1);
    Q_ASSERT(manager);
    m_handle = manager;
    d->newDialogListener.connect(&manager->events.new_dialog, [d](void *data) {
        d->onNewDialog(static_cast<wlr_xdg_dialog_v1 *>(data));
    });
}

void WXdgDialogManagerV1::destroy([[maybe_unused]] WServer *server)
{
    W_D(WXdgDialogManagerV1);
    d->newDialogListener.disconnect();
    d->dialogListeners.clear();
    m_handle = nullptr;
}

wl_global *WXdgDialogManagerV1::global() const
{
    return handle() ? handle()->global : nullptr;
}

WAYLIB_SERVER_END_NAMESPACE
