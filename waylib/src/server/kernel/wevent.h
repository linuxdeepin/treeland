// Copyright (C) 2024 groveer <guoyao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wglobal.h"

#include <QEvent>
#include <QInputEvent>
#include <QNativeGestureEvent>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WGestureEvent : public QNativeGestureEvent {
public:

    enum WLibInputGestureType {
        SwipeGesture,
        PinchGesture,
        HoldGesture
    };
    explicit WGestureEvent(WLibInputGestureType libInputGestureType, Qt::NativeGestureType nativeGestureType,
                const QPointingDevice *dev, int fingerCount, const QPointF &localPos,
                const QPointF &scenePos, const QPointF &globalPos,
                qreal value, const QPointF &delta, quint64 sequenceId = 0);
    inline WLibInputGestureType libInputGestureType() const { return m_libInputGestureType; }
    inline void setCancelled(const bool cancelled) { m_cancelled = cancelled; }
    inline bool cancelled() const { return m_cancelled; }

private:
    WLibInputGestureType m_libInputGestureType;
    bool m_cancelled;
};


WAYLIB_SERVER_END_NAMESPACE
