// Copyright (C) 2024 groveer <guoyao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "wevent.h"
#include <qnamespace.h>

WAYLIB_SERVER_BEGIN_NAMESPACE

WGestureEvent::WGestureEvent(WLibInputGestureType libInputGestureType, Qt::NativeGestureType nativeGestureType,
                const QPointingDevice *dev, int fingerCount, const QPointF &localPos,
                const QPointF &scenePos, const QPointF &globalPos,
                qreal value, const QPointF &delta, quint64 sequenceId)
    : QNativeGestureEvent(nativeGestureType, dev, fingerCount, localPos, scenePos, globalPos, value, delta, sequenceId)
    , m_libInputGestureType(libInputGestureType)
    , m_cancelled(false)
{

}

WAYLIB_SERVER_END_NAMESPACE
