// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Routes wlroots' C logging into the Qt logging system. Instead of letting
// wlroots write to stderr, every wlr_log call is forwarded to the "wlroots"
// QLoggingCategory so it honors QT_LOGGING_RULES filtering and Qt's message
// pattern.

#pragma once

#include <wglobal.h>
#include <wlr_all.h>

#include <QString>

WAYLIB_SERVER_BEGIN_NAMESPACE

class WAYLIB_SERVER_EXPORT WLog
{
public:
    // Install the default callback, which routes wlroots messages to the
    // "wlroots" Qt logging category.
    static void init(wlr_log_importance verbosity = WLR_DEBUG) {
        init(verbosity, &defaultLogCallback);
    }
    // Install a custom wlroots log callback (e.g. for testing or capture).
    static void init(wlr_log_importance verbosity, wlr_log_func_t callback) {
        wlr_log_init(verbosity, callback);
    }

private:
    static void defaultLogCallback(wlr_log_importance verbosity, const char *fmt, va_list args);
};

WAYLIB_SERVER_END_NAMESPACE
