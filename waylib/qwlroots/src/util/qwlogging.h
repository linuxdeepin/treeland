// Copyright (C) 2024 WenHao Peng <pengwenhao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwglobal.h>

extern "C" {
#include <wlr/util/log.h>
}

#include <QDebug>
#include <QLoggingCategory>

QW_BEGIN_NAMESPACE

class qw_log
{
#ifdef QT_DEBUG
    static Q_LOGGING_CATEGORY(lcQWLog, "wlroots", QtDebugMsg)
#else
    static Q_LOGGING_CATEGORY(lcQWLog, "wlroots", QtInfoMsg)
#endif

    static void default_log_callback(wlr_log_importance verbosity, const char *fmt, va_list args)
    {
        switch((int)verbosity) {
        case WLR_ERROR :
            qCCritical(lcQWLog) << QString::vasprintf(fmt, args);
            break;

        case WLR_INFO :
            qCInfo(lcQWLog) << QString::vasprintf(fmt, args);
            break;

        case WLR_DEBUG :
            qCDebug(lcQWLog) << QString::vasprintf(fmt, args);
            break;

        default:
            break;
        }
    }

public:
    QW_ALWAYS_INLINE static void init(wlr_log_importance verbosity = WLR_DEBUG) {
        init(verbosity, default_log_callback);
    }
    QW_ALWAYS_INLINE static void init(wlr_log_importance verbosity, wlr_log_func_t callback) {
        wlr_log_init(verbosity, callback);
    }
};

QW_END_NAMESPACE
