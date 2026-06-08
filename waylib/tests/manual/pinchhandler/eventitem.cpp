// Copyright (C) 2024 groveer <guoyao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "eventitem.h"

EventItem::EventItem(QQuickItem *parent) : QQuickItem(parent) {
  setAcceptHoverEvents(true);
  setAcceptTouchEvents(true);
  setAcceptedMouseButtons(Qt::AllButtons);
  setFlag(QQuickItem::ItemClipsChildrenToShape, true);
}

bool EventItem::event(QEvent *event) {
  switch (event->type()) {
    using enum QEvent::Type;
  // Don't insert events before MouseButtonPress
  case MouseButtonPress:
    Q_FALLTHROUGH();
  case MouseButtonRelease:
    Q_FALLTHROUGH();
  case MouseMove:
    Q_FALLTHROUGH();
  case HoverMove:
    if (static_cast<QMouseEvent *>(event)->source() !=
        Qt::MouseEventNotSynthesized)
      return true; // The non-native events don't send to WSeat
    Q_FALLTHROUGH();
  case HoverEnter:
    Q_FALLTHROUGH();
  case HoverLeave:
    Q_FALLTHROUGH();
  case KeyPress:
    Q_FALLTHROUGH();
  case KeyRelease:
    Q_FALLTHROUGH();
  case Wheel:
    Q_FALLTHROUGH();
  case TouchBegin:
    Q_FALLTHROUGH();
  case TouchUpdate:
    Q_FALLTHROUGH();
  case TouchEnd:
    Q_FALLTHROUGH();
  case TouchCancel: {
    auto e = static_cast<QInputEvent *>(event);
    Q_ASSERT(e);
    // We may receive HoverLeave when WSurfaceItem was destroying
    break;
  }
  case NativeGesture: {
    auto e = static_cast<QNativeGestureEvent *>(event);
    Q_ASSERT(e);
    // We may receive HoverLeave when WSurfaceItem was destroying
    qWarning() << "received app type: " << e->gestureType() << " delta: " << e->delta()
            << "value:" << e->value();
    break;
  }
  // No need to process focusOut, see [PR
  // 282](https://github.com/vioken/waylib/pull/282)
  default:
    break;
  }

  return QQuickItem::event(event);
}
