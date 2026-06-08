// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "wglobal.h"

#include <qwglobal.h>

#include <QPointer>
#define protected public
#include <qpa/qplatformintegration.h>
#undef protected
#include <qpa/qplatformvulkaninstance.h>
#include <qpa/qplatformnativeinterface.h>

QW_BEGIN_NAMESPACE
class QWDisplay;
QW_END_NAMESPACE

QW_USE_NAMESPACE

QT_BEGIN_NAMESPACE
class QInputDevice;
namespace QtWaylandClient {
class QWaylandIntegration;
}
QT_END_NAMESPACE

WAYLIB_SERVER_BEGIN_NAMESPACE

class WOutput;
class WInputDevice;
class QWlrootsScreen;
class Q_DECL_HIDDEN QWlrootsIntegration : public QPlatformIntegration, public QPlatformNativeInterface
{
public:
    QWlrootsIntegration(bool master, const QStringList &parameters, std::function<void()> onInitialized);
    ~QWlrootsIntegration();

    static QWlrootsIntegration *instance();

    void setProxy(QPlatformIntegration *proxy);

    QWlrootsScreen *addScreen(WOutput *output);
    void removeScreen(WOutput *output);
    QWlrootsScreen *getScreenFrom(const WOutput *output);
    inline const QList<QWlrootsScreen*> &screens() const {
        return m_screens;
    }

    QPointer<QInputDevice> addInputDevice(WInputDevice *device, const QString &seatName);
    bool removeInputDevice(WInputDevice *device);
    QInputDevice *getInputDeviceFrom(WInputDevice *device);

    void initialize() override;
    void destroy() override;

    bool hasCapability(QPlatformIntegration::Capability cap) const override;
    QPlatformFontDatabase *fontDatabase() const override;

    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
#ifndef QT_NO_OPENGL
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const override;
    QOpenGLContext::OpenGLModuleType openGLModuleType() override;
#endif
    QAbstractEventDispatcher *createEventDispatcher() const override;

    QPlatformNativeInterface *nativeInterface() const override;

    QPlatformPixmap *createPlatformPixmap(QPlatformPixmap::PixelType type) const override;
    QPlatformWindow *createForeignWindow(QWindow *, WId) const override;
    QPlatformSharedGraphicsCache *createPlatformSharedGraphicsCache(const char *cacheId) const override;
    QPaintEngine *createImagePaintEngine(QPaintDevice *paintDevice) const override;

#ifndef QT_NO_CLIPBOARD
    QPlatformClipboard *clipboard() const override;
#endif
#if QT_CONFIG(draganddrop)
    QPlatformDrag *drag() const override;
#endif
    QPlatformInputContext *inputContext() const override;
#if QT_CONFIG(accessibility)
    QPlatformAccessibility *accessibility() const override;
#endif

    QPlatformServices *services() const override;

    QVariant styleHint(StyleHint hint) const override;
    Qt::WindowState defaultWindowState(Qt::WindowFlags flags) const override;

    Qt::KeyboardModifiers queryKeyboardModifiers() const override;
    QList<int> possibleKeys(const QKeyEvent *) const override;

    QStringList themeNames() const override;
    QPlatformTheme *createPlatformTheme(const QString &name) const override;

    QPlatformOffscreenSurface *createPlatformOffscreenSurface(QOffscreenSurface *surface) const override;

#ifndef QT_NO_SESSIONMANAGER
    QPlatformSessionManager *createPlatformSessionManager(const QString &id, const QString &key) const override;
#endif

    void sync() override;
    void setApplicationIcon(const QIcon &icon) const override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    void setApplicationBadge(qint64 number) override;
#endif
    void beep() const override;
    void quit() const override;

#if QT_CONFIG(vulkan)
    QPlatformVulkanInstance *createPlatformVulkanInstance(QVulkanInstance *instance) const override;
#endif

    // native interfaces
    void *nativeResourceForScreen(const QByteArray &resource, QScreen *screen) override;

private:
    friend class QWlrootsScreen;

    mutable std::unique_ptr<QPlatformFontDatabase> m_fontDb;
    std::unique_ptr<QPlatformServices> m_services;
    std::unique_ptr<QPlatformPlaceholderScreen> m_placeholderScreen;

    QList<QWlrootsScreen*> m_screens;

private:
    inline bool isMaster() const {
        return m_master;
    }

    static QWlrootsIntegration *m_instance;
    bool m_master = true;
    std::function<void()> m_onInitialized;
    std::unique_ptr<QPlatformIntegration> m_proxyIntegration;
};

WAYLIB_SERVER_END_NAMESPACE
