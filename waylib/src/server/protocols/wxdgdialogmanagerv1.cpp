// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wxdgdialogmanagerv1.h"

#include "private/wglobal_p.h"
#include "wayliblogging.h"
#include "wxdgtoplevelsurface.h"

#include <wlr/types/wlr_xdg_dialog_v1.h>

#include <QPointer>

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
        return static_cast<wlr_xdg_wm_dialog_v1*>(q_func()->m_handle);
    }

    void onNewDialog(wlr_xdg_dialog_v1 *dialog);

    WScopedListener m_newDialogListener;

    struct DialogState {
        wlr_xdg_dialog_v1 *dialog;
        WScopedListener setModalListener;
        WScopedListener destroyListener;
    };
    QList<DialogState*> dialogStates;

    W_DECLARE_PUBLIC(WXdgDialogManagerV1)
};

void WXdgDialogManagerV1Private::onNewDialog(wlr_xdg_dialog_v1 *nativeDialog)
{
    W_Q(WXdgDialogManagerV1);

    auto *surface = WXdgToplevelSurface::fromHandle(nativeDialog->xdg_toplevel);
    if (!surface) {
        qCWarning(lcWlXdgDialog) << "Failed to get WXdgToplevelSurface from dialog";
        return;
    }

    qCDebug(lcWlXdgDialog) << "New xdg_dialog_v1 for surface:" << surface
                           << "initial modal:" << nativeDialog->modal;

    // Wrap in QPointer to guard against toplevel being destroyed before the dialog.
    QPointer<WXdgToplevelSurface> surfaceGuard(surface);

    // Sync the initial modal state (client may have called set_modal before we connect)
    if (nativeDialog->modal && surfaceGuard)
        Q_EMIT q->surfaceModalChanged(surfaceGuard, true);

    auto *state = new DialogState{nativeDialog, {}, {}};
    dialogStates.append(state);

    // Track future set_modal / unset_modal calls. Both requests fire set_modal event.
    state->setModalListener.connect(&nativeDialog->events.set_modal,
                     [q, nativeDialog, surfaceGuard](wl_listener *, void *) {
                         if (!surfaceGuard)
                             return;
                         qCDebug(lcWlXdgDialog) << "xdg_dialog_v1 modal changed:" << surfaceGuard
                                                << "->" << nativeDialog->modal;
                         Q_EMIT q->surfaceModalChanged(surfaceGuard, nativeDialog->modal);
                     });

    // When the dialog object is destroyed, reset modal if it was set.
    state->destroyListener.connect(&nativeDialog->events.destroy,
                     [this, state, q, nativeDialog, surfaceGuard](wl_listener *, void *) {
                         if (nativeDialog->modal && surfaceGuard) {
                             qCDebug(lcWlXdgDialog)
                                 << "xdg_dialog_v1 destroyed while modal, resetting:"
                                 << surfaceGuard;
                             Q_EMIT q->surfaceModalChanged(surfaceGuard, false);
                         }
                         dialogStates.removeOne(state);
                         delete state;
                     });
}

WXdgDialogManagerV1::WXdgDialogManagerV1(QObject *parent)
    : QObject(parent)
    , WObject(*new WXdgDialogManagerV1Private(this))
{
}

WXdgDialogManagerV1::~WXdgDialogManagerV1() = default;

QByteArrayView WXdgDialogManagerV1::interfaceName() const
{
    return "xdg_wm_dialog_v1";
}

void WXdgDialogManagerV1::create(WServer *server)
{
    W_D(WXdgDialogManagerV1);
    m_handle = wlr_xdg_wm_dialog_v1_create(server->handle(), 1);
    auto *wm = static_cast<wlr_xdg_wm_dialog_v1*>(m_handle);

    d->m_newDialogListener.connect(&wm->events.new_dialog, [this](wl_listener *, void *data) {
        W_D(WXdgDialogManagerV1);
        d->onNewDialog(static_cast<wlr_xdg_dialog_v1*>(data));
    });
}

void WXdgDialogManagerV1::destroy([[maybe_unused]] WServer *server)
{
    W_D(WXdgDialogManagerV1);
    d->m_newDialogListener.remove();
    for (auto *state : std::as_const(d->dialogStates)) {
        delete state;
    }
    d->dialogStates.clear();
}

wl_global *WXdgDialogManagerV1::global() const
{
    W_DC(WXdgDialogManagerV1);
    if (!d->handle())
        return nullptr;
    return d->handle()->global;
}

WAYLIB_SERVER_END_NAMESPACE
