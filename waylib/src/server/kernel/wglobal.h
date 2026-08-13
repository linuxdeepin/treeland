// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wconfig.h>
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

#include <QScopedPointer>
#include <QList>
#include <QObject>
#include <QThread>

#include <wlr_all.h>

#include <functional>
#include <type_traits>

struct wl_client;
WAYLIB_SERVER_BEGIN_NAMESPACE

class WClient;

class WObjectPrivate;
class WScopedListenerList;
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

    // Default construction for mixin use (e.g. QObject + WObject) without a
    // dedicated *Private subclass.
    WObject();

    // Listener list for this wrapper's own native handle signals.
    // Prefer this over listeners(this).
    WScopedListenerList *listeners();

    // Register wl_signal listeners on another wrapper. `owner` groups listeners
    // for later removal (see removeListeners). When owner is another WObject,
    // teardown() on the owner automatically detaches its listener groups from
    // every target it registered on.
    //
    // Do NOT pass `this` here — use listeners() for this object's own
    // listener list. listeners(this) is rejected at runtime.
    WScopedListenerList *listeners(WObject *owner);
    void removeListeners(WObject *owner);

    // Detach and release every listener list owned by this object. Safe to
    // call multiple times. Derived classes that register listeners MUST call
    // this in their destructor (or earlier, e.g. before destroying native
    // handles). WServer::stop()/detach() also call this for interfaces that
    // are WObject before WServerInterface::destroy(). ~WObject aborts if any
    // listener groups or cross-object targets remain.
    void teardown();

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

// Lightweight owner token for WObject::listeners(owner) when the registering
// object is not itself a WObject (or should not expose WObject publicly).
// Public destructor allows std::unique_ptr<WListenerOwner>.
class WAYLIB_SERVER_EXPORT WListenerOwner final : public WObject
{
public:
    WListenerOwner() : WObject() {}
    // unique_ptr-friendly public dtor; always clear listener ownership.
    ~WListenerOwner() { teardown(); }
};

class WAYLIB_SERVER_EXPORT WGlobal {
    Q_GADGET
    QML_VALUE_TYPE(wglobal)
    QML_UNCREATABLE("Use for enums")

public:
    enum class ColorContentsMode {
        DontCare,
        Clear,
        Preserve,
    };
    Q_ENUM(ColorContentsMode)

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
        ZoomOut,
        DndAsk,
        AllResize
    };
    Q_ENUM(CursorShape)
    static_assert(CursorShape::BottomLeftCorner > CursorShape::Default, "");

    static bool isInvalidCursor(const QCursor &c);
    static bool isClientResourceCursor(const QCursor &c);
};

struct Q_DECL_HIDDEN WCursorShapeQMLProxy {
    Q_GADGET
    QML_NAMED_ELEMENT(CursorShape)
    QML_FOREIGN_NAMESPACE(WAYLIB_SERVER_NAMESPACE::WGlobal)
    QML_UNCREATABLE("Use for enums")
};

WAYLIB_SERVER_END_NAMESPACE
