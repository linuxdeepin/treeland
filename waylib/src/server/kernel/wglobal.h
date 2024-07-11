// Copyright (C) 2023 JiDe Zhang <zhangjide@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <qwconfig.h>
#include <qtguiglobal.h>
#include <QtQmlIntegration>

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

#ifndef WLR_HAVE_XWAYLAND
#ifndef DISABLE_XWAYLAND
#define DISABLE_XWAYLAND
#endif
#endif

#include <qwglobal.h>
#include <qwobject.h>
#include <QScopedPointer>
#include <QList>
#include <QObject>
#include <QThread>

#include <type_traits>

struct wl_client;
WAYLIB_SERVER_BEGIN_NAMESPACE

class WClient;
class WObjectPrivate;
class WAYLIB_SERVER_EXPORT WObject
{
public:
    template<typename T>
    T *getAttachedData(const void *owner) const {
        void *data = attachedDatas().value(indexOfAttachedData(owner)).second;
        return reinterpret_cast<T*>(data);
    }
    template<typename T>
    T *getAttachedData() const {
        const void *owner = typeid(T).name();
        return getAttachedData<T>(owner);
    }

    template<typename T>
    void setAttachedData(const void *owner, void *data) {
        Q_ASSERT(indexOfAttachedData(owner) < 0);
        attachedDatas().append({owner, data});
    }
    template<typename T>
    void setAttachedData(void *data) {
        const void *owner = typeid(T).name();
        setAttachedData<T>(owner, data);
    }

    template<typename T>
    void removeAttachedData(const void *owner) {
        int index = indexOfAttachedData(owner);
        Q_ASSERT(index >= 0);
        attachedDatas().removeAt(index);
    }
    template<typename T>
    void removeAttachedData() {
        const void *owner = typeid(T).name();
        removeAttachedData<T>(owner);
    }

    WClient *waylandClient() const;

protected:
    WObject(WObjectPrivate &dd, WObject *parent = nullptr);

    int indexOfAttachedData(const void *owner) const;
    const QList<std::pair<const void*, void*>> &attachedDatas() const;
    QList<std::pair<const void*, void*>> &attachedDatas();

    virtual ~WObject();
    QScopedPointer<WObjectPrivate> w_d_ptr;

    Q_DISABLE_COPY(WObject)
    W_DECLARE_PRIVATE(WObject)
};

class WWrapObjectPrivate;
// Wrap Object in QWlroots
class WAYLIB_SERVER_EXPORT WWrapObject : public QObject,  public WObject
{
    Q_OBJECT

public:
    QW_NAMESPACE::qw_object_basic *handle() const;
    bool isInvalidated() const;

    bool safeDisconnect(const QObject *receiver);
    bool safeDisconnect(const QMetaObject::Connection &connection);

    void safeDeleteLater();

Q_SIGNALS:
    void aboutToBeInvalidated();
    void invalidated();

public:
    template<typename Func1, typename Func2>
    inline typename std::enable_if<std::is_base_of<WWrapObject, typename QtPrivate::FunctionPointer<Func1>::Object>::value ||
                                       std::is_same<QObject, typename QtPrivate::FunctionPointer<Func1>::Object>::value, QMetaObject::Connection>::type
    safeConnect(Func1 signal, const QObject *receiver, Func2 slot, Qt::ConnectionType type = Qt::AutoConnection) {
        return QObject::connect(qobject_cast<typename QtPrivate::FunctionPointer<Func1>::Object*>(this), signal, receiver, slot, type);
    }

    template<typename Func1, typename Func2>
    typename std::enable_if<std::is_base_of<QW_NAMESPACE::qw_object_basic, typename QtPrivate::FunctionPointer<Func1>::Object>::value, QMetaObject::Connection>::type
    safeConnect(Func1 signal, const QObject *receiver, Func2 slot, Qt::ConnectionType type = Qt::AutoConnection) {
        // Isn't thread safety
        Q_ASSERT(QThread::currentThread() == thread());
        Q_ASSERT_X(this != receiver, "safeConnect",
                   "Not need to use safeConnect for the signal of self's handle object,"
                   " Please use QObject::connect().");

        if constexpr (std::is_same_v<decltype(signal), decltype(&QObject::destroyed)>) {
            // Post warning in compilation time
            int The_Connect_Maybe_Invalid_Bacause_Maybe_Disconnect_After_T_beforeDestroy;
        }

        beginSafeConnect();
        auto h = qobject_cast<typename QtPrivate::FunctionPointer<Func1>::Object*>(handle());
        Q_ASSERT(h);
        auto connection = QObject::connect(h, signal, receiver, slot, type);
        endSafeConnect(connection);

        return connection;
    }

protected:
    WWrapObject(QObject *parent = nullptr);
    WWrapObject(WWrapObjectPrivate &dd, QObject *parent = nullptr);
    virtual ~WWrapObject() override;
    using QObject::connect;
    using QObject::disconnect;
    using QObject::deleteLater;

    void invalidate();
    void initHandle(QW_NAMESPACE::qw_object_basic *handle);

    void beginSafeConnect();
    void endSafeConnect(const QMetaObject::Connection &connection);

#ifdef QT_DEBUG
    bool event(QEvent *event) override;
#endif

    W_DECLARE_PRIVATE(WWrapObject)
};

class WAYLIB_SERVER_EXPORT WGlobal {
    Q_GADGET
    QML_NAMED_ELEMENT(Waylib)
    QML_UNCREATABLE("Use for enums")

public:
    enum class CursorShape {
        Default = Qt::CustomCursor + 1,
        Invalid,
        ClientResource,
        BottomLeftCorner,
        BottomRightCorner,
        TopLeftCorner,
        TopRightCorner,
        BottomSide,
        LeftSide,
        RightSide,
        TopSide,
        Grabbing,
        Xterm,
        Hand1,
        Watch,
        SWResize,
        SEResize,
        SResize,
        WResize,
        EResize,
        EWResize,
        NWResize,
        NWSEResize,
        NEResize,
        NESWResize,
        NSResize,
        NResize,
        AllScroll,
        Text,
        Pointer,
        Wait,
        ContextMenu,
        Help,
        Progress,
        Cell,
        Crosshair,
        VerticalText,
        Alias,
        Copy,
        Move,
        NoDrop,
        NotAllowed,
        Grab,
        ColResize,
        RowResize,
        ZoomIn,
        ZoomOut
    };
    Q_ENUM(CursorShape)
    static_assert(CursorShape::BottomLeftCorner > CursorShape::Default, "");

    static bool isInvalidCursor(const QCursor &c);
    static bool isClientResourceCursor(const QCursor &c);
};

WAYLIB_SERVER_END_NAMESPACE
