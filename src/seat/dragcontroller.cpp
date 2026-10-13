// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "dragcontroller.h"
#include "seatmanager.h"
#include "modules/dde-shell/ddeshellmanagerinterfacev1.h"
#include "modules/active-notify/activenotifymanagerinterfacev1.h"

#include <wcursor.h>
#include <wseat.h>

#include <wlr_all.h>

DragController::DragController(SeatManager *seatManager, QObject *parent)
    : QObject(parent)
    , m_seatManager(seatManager)
{}

DragController::~DragController() = default;

void DragController::initialize(bool notifyDdeShell)
{
    m_notifyDdeShell = notifyDdeShell;
    for (auto *seat : m_seatManager->seats())
        trackSeat(seat);
}

void DragController::trackSeat(WSeat *seat)
{
    disconnect(seat, &WSeat::requestDrag, this, nullptr);
    connect(seat, &WSeat::requestDrag, this, [this, seat](WSurface *surface) {
        handleRequestDrag(seat, surface);
    });
}

void DragController::handleRequestDrag(WSeat *seat, WSurface *)
{
    if (!seat || !seat->handle())
        return;

    seat->setAlwaysUpdateHoverTarget(true);
    struct wlr_drag *drag = seat->handle()->drag;
    Q_ASSERT(drag);

    SeatDragEntry entry;
    entry.drag = drag;
    entry.owner = std::make_unique<WListenerOwner>();
    auto *dragOwner = entry.owner.get();
    seat->listeners(dragOwner)->add(&drag->events.drop, this, [this, seat] (void *) {
        if (m_notifyDdeShell)
            DDEActiveInterface::sendDrop(seat);
        ActiveNotifyV1::sendDragChanged(ActiveNotifyV1::Dropped, seat);
    });

    QPointer<WCursor> dragCursor = seat->cursor();
    seat->listeners(dragOwner)->add(&drag->events.destroy, this, [this, seat, drag, dragCursor, dragOwner] (void *) {
        if (dragCursor)
            dragCursor->setOverrideCursor(WCursor::toQCursor(WGlobal::CursorShape::Invalid));
        if (!drag->dropped)
            ActiveNotifyV1::sendDragChanged(ActiveNotifyV1::Cancelled, seat);
        drag->data = NULL;
        seat->setAlwaysUpdateHoverTarget(false);
        // Drop the drop/destroy/dnd_action listener entries while the destroy
        // signal is being emitted: wlr_drag.c drag_destroy asserts the drop
        // and destroy lists are empty after (erasing the entry destroys its
        // WScopedListenerList and disconnects all of them).
        seat->removeListeners(dragOwner);
        std::erase_if(seatDragEntries, [drag](const auto &e) {
            return e.drag == drag;
        });
    });

    // TODO: https://gitlab.freedesktop.org/wayland/wayland/-/work_items/444
    const auto updateCursor = [drag, dragCursor] {
        if (!dragCursor || drag->grab_type != WLR_DRAG_GRAB_KEYBOARD_POINTER)
            return;

        const auto action = drag->source
                ? drag->source->current_dnd_action
                : WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
        auto shape = WGlobal::CursorShape::NoDrop;
        if (action == WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
            shape = WGlobal::CursorShape::Copy;
        else if (action == WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
            shape = WGlobal::CursorShape::Move;
        else if (action == WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
            shape = WGlobal::CursorShape::DndAsk;
        dragCursor->setOverrideCursor(WCursor::toQCursor(shape));
    };

    if (drag->source) {
        seat->listeners(dragOwner)->add(&drag->source->events.dnd_action, this, updateCursor);
    }
    updateCursor();

    // Move the bundle into the list only after dndAction/updateCursor above.
    seatDragEntries.push_back(std::move(entry));

    if (m_notifyDdeShell)
        DDEActiveInterface::sendStartDrag(seat);
    ActiveNotifyV1::sendDragChanged(ActiveNotifyV1::Started, seat);
}
