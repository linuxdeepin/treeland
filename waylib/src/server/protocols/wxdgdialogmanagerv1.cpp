// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgdialogmanagerv1.h"

#include "private/wglobal_p.h"
#include "wscoplistener.h"
#include "wayliblogging.h"
#include "wxdgtoplevelsurface.h"

#include <wlr_all.h>

#include <QPointer>

#include <memory>
#include <vector>

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WXdgDialogManagerV1Private : public WObjectPrivate
{
public:
    WXdgDialogManagerV1Private(WXdgDialogManagerV1 *qq)
        : WObjectPrivate(qq)
    {
    }

    inline wlr_xdg_wm_dialog_v1 *handle() const
    {
        return reinterpret_cast<wlr_xdg_wm_dialog_v1*>(q_func()->m_handle);
    }

    void onNewDialog(wlr_xdg_dialog_v1 *dialog);

    W_DECLARE_PUBLIC(WXdgDialogManagerV1)

    // Per-dialog WListenerOwner: dialog signals are registered on the
    // owner token (not the surface). Manager owns each group via
    // owner->listeners(q), so items stay isolated and manager teardown
    // clears them through the cross-object listener graph.
    struct DialogListeners {
        wlr_xdg_dialog_v1 *dialog = nullptr;
        std::unique_ptr<WListenerOwner> owner;
    };
    std::vector<DialogListeners> dialogListeners;
};

void WXdgDialogManagerV1Private::onNewDialog(wlr_xdg_dialog_v1 *nativeDialog)
{
    W_Q(WXdgDialogManagerV1);

    auto *surface = WXdgToplevelSurface::fromHandle(nativeDialog->xdg_toplevel);
    if (!surface) {
        qCWarning(lcWlXdgDialog) << "Failed to get WXdgToplevelSurface from" << nativeDialog->xdg_toplevel;
        return;
    }

    qCDebug(lcWlXdgDialog) << "New xdg_dialog_v1 for surface:" << surface
                           << "initial modal:" << nativeDialog->modal;

    // Wrap in QPointer to guard against toplevel being destroyed before the dialog.
    // surface is a WXdgToplevelSurface* (QObject*), so QPointer safely nulls when destroyed.
    QPointer<WXdgToplevelSurface> surfaceGuard(surface);

    // Sync the initial modal state (client may have called set_modal before we connect)
    if (nativeDialog->modal && surfaceGuard)
        Q_EMIT q->surfaceModalChanged(surfaceGuard, true);

    DialogListeners entry;
    entry.dialog = nativeDialog;
    entry.owner = std::make_unique<WListenerOwner>();
    auto *owner = entry.owner.get();

    // Listen to dialog signals on the owner token; q (the manager) owns the
    // group so teardown()/removeListeners(q) detaches without touching other
    // dialogs' owners.
    // Track future set_modal / unset_modal calls. Both requests fire the set_modal event.
    owner->listeners(q)->add(&nativeDialog->events.set_modal, this,
                     [q, nativeDialog, surfaceGuard] (void *) {
                         if (!surfaceGuard)
                             return;
                         qCDebug(lcWlXdgDialog) << "xdg_dialog_v1 modal changed:" << surfaceGuard
                                                << "->" << nativeDialog->modal;
                         Q_EMIT q->surfaceModalChanged(surfaceGuard, nativeDialog->modal);
                     });

    // When the dialog object is destroyed, reset modal if it was set.
    // Protocol: "If this object is destroyed before the related xdg_toplevel,
    // the compositor should unapply its effects."
    owner->listeners(q)->add(&nativeDialog->events.destroy, this,
                     [this, q, nativeDialog, surfaceGuard, owner] (void *) {
                         if (nativeDialog->modal && surfaceGuard) {
                             qCDebug(lcWlXdgDialog)
                                 << "xdg_dialog_v1 destroyed while modal, resetting:"
                                 << surfaceGuard;
                             Q_EMIT q->surfaceModalChanged(surfaceGuard, false);
                         }
                         owner->removeListeners(q);
                         for (auto it = dialogListeners.begin(); it != dialogListeners.end(); ++it) {
                             if (it->dialog == nativeDialog) {
                                 dialogListeners.erase(it);
                                 break;
                             }
                         }
                     });
    dialogListeners.push_back(std::move(entry));
}

WXdgDialogManagerV1::WXdgDialogManagerV1(QObject *parent)
    : QObject(parent)
    , WObject(*new WXdgDialogManagerV1Private(this))
{
}

WXdgDialogManagerV1::~WXdgDialogManagerV1()
{
    teardown();
}

QByteArrayView WXdgDialogManagerV1::interfaceName() const
{
    return "xdg_wm_dialog_v1";
}

wlr_xdg_wm_dialog_v1 *WXdgDialogManagerV1::handle() const
{
    return reinterpret_cast<wlr_xdg_wm_dialog_v1*>(m_handle);
}

void WXdgDialogManagerV1::create(WServer *server)
{
    auto *wm = wlr_xdg_wm_dialog_v1_create(server->handle(), InterfaceVersion);
    m_handle = wm;

    W_D(WXdgDialogManagerV1);
    listeners()->add(&wm->events.new_dialog, this, [this](wlr_xdg_dialog_v1 *dialog) {
        W_D(WXdgDialogManagerV1);
        d->onNewDialog(dialog);
    });
}

void WXdgDialogManagerV1::destroy([[maybe_unused]] WServer *server)
{
    W_D(WXdgDialogManagerV1);
    // Manager-owned listeners were already dropped by WServer teardown.
    // Clearing dialogListeners destroys per-dialog WListenerOwner tokens.
    d->dialogListeners.clear();
    // Clear the dangling handle now: the wlr_xdg_wm_dialog_v1 is reclaimed
    // by display.reset() in WServer::stop(), but nulling m_handle immediately
    // makes handle()/global() return null instead of dangling (global() already
    // guards on handle()).
    m_handle = nullptr;
}

wl_global *WXdgDialogManagerV1::global() const
{
    W_DC(WXdgDialogManagerV1);
    if (!d->handle())
        return nullptr;
    return d->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
