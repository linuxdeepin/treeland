// Copyright (C) 2023-2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wlr_fwd.h>
#include <wglobal.h>
#include <wtypes.h>

#include <QObject>
#include <QSize>
#include <QPoint>
#include <QQmlEngine>
#include <QImage>

Q_MOC_INCLUDE("wcursor.h")

QT_BEGIN_NAMESPACE
class QScreen;
class QQuickWindow;
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class QWlrootsScreen;
class QWlrootsIntegration;

class WOutputLayout;
class WCursor;
class WBackend;
class WServer;
class WOutputPrivate;
class WBackendPrivate;
class WAYLIB_SERVER_EXPORT WOutput : public QObject, public WObject
{
    Q_OBJECT
    W_DECLARE_PRIVATE(WOutput)
    Q_PROPERTY(bool enabled READ isEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QSize size READ effectiveSize NOTIFY effectiveSizeChanged)
    Q_PROPERTY(Transform orientation READ orientation NOTIFY orientationChanged)
    Q_PROPERTY(float scale READ scale NOTIFY scaleChanged)
    Q_PROPERTY(bool forceSoftwareCursor READ forceSoftwareCursor WRITE setForceSoftwareCursor NOTIFY forceSoftwareCursorChanged)
    Q_PROPERTY(QString name READ name CONSTANT)
    QML_NAMED_ELEMENT(WaylandOutput)
    QML_UNCREATABLE("Can't create in qml")

public:
    enum Transform {
        Normal = WLR::Transform::Normal,
        R90 = WLR::Transform::R90,
        R180 = WLR::Transform::R180,
        R270 = WLR::Transform::R270,
        Flipped = WLR::Transform::Flipped,
        Flipped90 = WLR::Transform::Flipped90,
        Flipped180 = WLR::Transform::Flipped180,
        Flipped270 = WLR::Transform::Flipped270
    };
    Q_ENUM(Transform)

    explicit WOutput(wlr_output *handle, WBackend *backend);
    ~WOutput();

    WBackend *backend() const;
    WServer *server() const;
    wlr_renderer *renderer() const;
    wlr_swapchain *swapchain() const;
    wlr_allocator *allocator() const;
    bool configurePrimarySwapchain(const QSize &size, uint32_t format,
                                   wlr_swapchain **swapchain,
                                   bool doTest = true);
    bool configureCursorSwapchain(const QSize &size, uint32_t format,
                                  wlr_swapchain **swapchain);

    wlr_output *handle() const;

    static WOutput *fromHandle(wlr_output *handle);

    static WOutput *fromScreen(const QScreen *screen);

    QString name() const;
    bool isEnabled() const;
    QPoint position() const;
    QSize size() const;
    QSize transformedSize() const;
    QSize effectiveSize() const;
    Transform orientation() const;
    float scale() const;

    void attach(QQuickWindow *window);
    QQuickWindow *attachedWindow() const;

    void setLayout(WOutputLayout *layout);
    WOutputLayout *layout() const;

    void addCursor(WCursor *cursor);
    void removeCursor(WCursor *cursor);
    const QList<WCursor *> &cursorList() const;

    bool forceSoftwareCursor() const;
    void setForceSoftwareCursor(bool on);

    void scheduleFrame();

Q_SIGNALS:
    void enabledChanged();
    void positionChanged(const QPoint &pos);
    void modeChanged();
    void transformedSizeChanged();
    // Emitted from the destructor while the object is still usable.
    void beforeDestroy();
    void effectiveSizeChanged();
    void orientationChanged();
    void scaleChanged();
    void forceSoftwareCursorChanged();
    void bufferCommitted();
    void cursorAdded(WAYLIB_SERVER_NAMESPACE::WCursor *cursor);
    void cursorRemoved(WAYLIB_SERVER_NAMESPACE::WCursor *cursor);
    void cursorListChanged();

private:
    friend class QWlrootsIntegration;
    void setScreen(QWlrootsScreen *screen);
    QWlrootsScreen *screen() const;

    friend class WServerPrivate;
    friend class WBackendPrivate;

    // Owned by the backend: released with `delete` from the native destroy
    // callback, never with deleteLater().
    using QObject::deleteLater;
};

WAYLIB_SERVER_END_NAMESPACE
Q_DECLARE_METATYPE(WAYLIB_SERVER_NAMESPACE::WOutput*)
