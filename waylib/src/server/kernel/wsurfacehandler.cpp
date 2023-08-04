// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "private/wsurfacehandler_p.h"
#include "woutput.h"
#include "wseat.h"
#include "wcursor.h"
#include "wsurface.h"

#include <qwoutputlayout.h>

#include <QMouseEvent>

WAYLIB_SERVER_BEGIN_NAMESPACE

WSurfaceHandler::WSurfaceHandler(WSurface *surface)
    : WSurfaceHandler(*new WSurfaceHandlerPrivate(surface, this))
{

}

WSurfaceHandler::WSurfaceHandler(WSurfaceHandlerPrivate &dd)
    : WObject(dd)
{

}

WAYLIB_SERVER_END_NAMESPACE
