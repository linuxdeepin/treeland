// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwconfig.h>
#include <qtguiglobal.h>

#define SERVER_NAMESPACE Server
#define WAYLIB_SERVER_NAMESPACE Waylib::SERVER_NAMESPACE

#ifndef SERVER_NAMESPACE
#   define WAYLIB_SERVER_BEGIN_NAMESPACE
#   define WAYLIB_SERVER_END_NAMESPACE
#   define WAYLIB_SERVER_USE_NAMESPACE
#else
#   define WAYLIB_SERVER_BEGIN_NAMESPACE namespace Waylib { namespace SERVER_NAMESPACE {
#   define WAYLIB_SERVER_END_NAMESPACE }}
#   define WAYLIB_SERVER_USE_NAMESPACE using namespace WAYLIB_SERVER_NAMESPACE;
#endif

#if defined(WAYLIB_STATIC_LIB)
#  define WAYLIB_SERVER_EXPORT
#else
#if defined(LIBWAYLIB_SERVER_LIBRARY)
#  define WAYLIB_SERVER_EXPORT Q_DECL_EXPORT
#else
#  define WAYLIB_SERVER_EXPORT Q_DECL_IMPORT
#endif
#endif

#define W_DECLARE_PRIVATE(Class) Q_DECLARE_PRIVATE_D(qGetPtrHelper(w_d_ptr),Class)
#define W_DECLARE_PUBLIC(Class) Q_DECLARE_PUBLIC(Class)
#define W_D(Class) Q_D(Class)
#define W_Q(Class) Q_Q(Class)
#define W_DC(Class) Q_D(const Class)
#define W_QC(Class) Q_Q(const Class)
#define W_PRIVATE_SLOT(Func) Q_PRIVATE_SLOT(d_func(), Func)

#if defined(WLR_HAVE_VULKAN_RENDERER) && QT_CONFIG(vulkan) && WLR_VERSION_MINOR > 16
#define ENABLE_VULKAN_RENDER
#endif

#include <QScopedPointer>

struct wl_client;
WAYLIB_SERVER_BEGIN_NAMESPACE

class WObjectPrivate;
class WAYLIB_SERVER_EXPORT WObject
{
protected:
    WObject(WObjectPrivate &dd, WObject *parent = nullptr);

    virtual ~WObject();

    QScopedPointer<WObjectPrivate> w_d_ptr;

    Q_DISABLE_COPY(WObject)
    W_DECLARE_PRIVATE(WObject)
};

class WObjectPrivate
{
public:
    static WObjectPrivate *get(WObject *qq) {
        return qq->d_func();
    }

    virtual ~WObjectPrivate();
    virtual wl_client *waylandClient() const {
        return nullptr;
    }

protected:
    WObjectPrivate(WObject *qq);

    WObject *q_ptr;

    Q_DECLARE_PUBLIC(WObject)
};

WAYLIB_SERVER_END_NAMESPACE
