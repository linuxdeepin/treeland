// Copyright (C) 2023 - 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwlrootsintegration.h"
#include "qwlrootscreen.h"
#include "qwlrootswindow.h"
#include "woutput.h"
#include "winputdevice.h"
#include "types.h"

#include <qwoutput.h>
#include <qwrenderer.h>
#include <qwinputdevice.h>

#include <QOffscreenSurface>
#include <QGuiApplication>

#include <private/qgenericunixfontdatabase_p.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
#include <private/qdesktopunixservices_p.h>
#else
#include <private/qgenericunixservices_p.h>
#endif

#include <private/qgenericunixeventdispatcher_p.h>
#include <private/qhighdpiscaling_p.h>
#if QT_CONFIG(vulkan)
#include <private/qvulkaninstance_p.h>
#include <private/qbasicvulkanplatforminstance_p.h>
#include <private/qguiapplication_p.h>
#endif
#include <private/qinputdevice_p.h>
#include <qpa/qplatformsurface.h>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformoffscreensurface.h>
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#include <private/qgenericunixtheme_p.h>
#else
#include <private/qgenericunixthemes_p.h>
#endif

#ifndef QT_NO_OPENGL
#include <qpa/qplatformopenglcontext.h>
#include <private/qeglconvenience_p.h>
#include <EGL/egl.h>

extern "C" {
#include <wlr/render/egl.h>
#define static
#include <wlr/render/gles2.h>
#undef static
}

#endif // QT_NO_OPENGL

WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN PlatformOffscreenSurface : public QPlatformOffscreenSurface
{
public:
    PlatformOffscreenSurface(QOffscreenSurface *surface)
        : QPlatformOffscreenSurface(surface) {}

    bool isValid() const override {
        return true;
    }

    QPlatformScreen *screen() const override {
        return nullptr;
    }
};

QWlrootsIntegration *QWlrootsIntegration::m_instance = nullptr;
QWlrootsIntegration::QWlrootsIntegration([[maybe_unused]] const QStringList &parameters, std::function<void ()> onInitialized)
    : m_onInitialized(onInitialized)
{
    Q_ASSERT(!m_instance);
    m_instance = this;
}

QWlrootsIntegration::~QWlrootsIntegration()
{
    if (m_instance == this)
        m_instance = nullptr;
}

QWlrootsIntegration *QWlrootsIntegration::instance()
{
    return m_instance;
}

QWlrootsScreen *QWlrootsIntegration::addScreen(WOutput *output)
{
    m_screens << new QWlrootsScreen(output);

    QWindowSystemInterface::handleScreenAdded(m_screens.last());

    if (m_placeholderScreen) {
        QWindowSystemInterface::handleScreenRemoved(m_placeholderScreen.release());
    }

    m_screens.last()->initialize();
    output->setScreen(m_screens.last());

    return m_screens.last();
}

void QWlrootsIntegration::removeScreen(WOutput *output)
{
    if (auto screen = output->screen()) {
        output->setScreen(nullptr);
        bool ok = m_screens.removeOne(screen);

        if (ok && m_screens.isEmpty()) {
            m_placeholderScreen.reset(new QPlatformPlaceholderScreen);
            QWindowSystemInterface::handleScreenAdded(m_placeholderScreen.get(), true);
        }

        QWindowSystemInterface::handleScreenRemoved(screen);
    }
}

QWlrootsScreen *QWlrootsIntegration::getScreenFrom(const WOutput *output)
{
    return output->screen();
}

QPointer<QInputDevice> QWlrootsIntegration::addInputDevice(WInputDevice *device, const QString &seatName)
{
    QPointer<QInputDevice> qtdev;
    auto qwDevice = device->handle();
    const QString name = QString::fromUtf8(qwDevice->handle()->name);
    qint64 systemId = reinterpret_cast<qint64>(device);

    switch (qwDevice->handle()->type) {
    case WLR_INPUT_DEVICE_KEYBOARD: {
        qtdev = new QInputDevice(name, systemId, QInputDevice::DeviceType::Keyboard, seatName);
        break;
    }
    case WLR_INPUT_DEVICE_POINTER: {
        qtdev = new QPointingDevice(name, systemId, QInputDevice::DeviceType::TouchPad, QPointingDevice::PointerType::Generic,
                                    QInputDevice::Capability::Position | QInputDevice::Capability::Hover
                                        | QInputDevice::Capability::Scroll | QInputDevice::Capability::MouseEmulation,
                                    10, 32, seatName, QPointingDeviceUniqueId());
        break;
    }
    case WLR_INPUT_DEVICE_TOUCH: {
        qtdev = new QPointingDevice(name, systemId, QInputDevice::DeviceType::TouchScreen, QPointingDevice::PointerType::Finger,
                                    QInputDevice::Capability::Position | QInputDevice::Capability::Area | QInputDevice::Capability::MouseEmulation,
                                    10, 32, seatName, QPointingDeviceUniqueId());
        break;
    }
    case WLR_INPUT_DEVICE_TABLET: {
        qtdev = new QPointingDevice(name, systemId, QInputDevice::DeviceType::Stylus, QPointingDevice::PointerType::Pen,
                                    QInputDevice::Capability::XTilt | QInputDevice::Capability::YTilt | QInputDevice::Capability::Pressure,
                                    1, 32, seatName, QPointingDeviceUniqueId());
        break;
    }
    case WLR_INPUT_DEVICE_TABLET_PAD: {
        auto pad = wlr_tablet_pad_from_input_device(qwDevice->handle());
        qtdev = new QPointingDevice(name, systemId, QInputDevice::DeviceType::TouchPad, QPointingDevice::PointerType::Pen,
                                    QInputDevice::Capability::Position | QInputDevice::Capability::Hover | QInputDevice::Capability::Pressure,
                                    1, pad->button_count, seatName, QPointingDeviceUniqueId());
        break;
    }
    case WLR_INPUT_DEVICE_SWITCH: {
        qtdev = new QInputDevice(name, systemId, QInputDevice::DeviceType::Keyboard, seatName);
        break;
    }
    }

    if (qtdev) {
        device->setQtDevice(qtdev);
        QWindowSystemInterface::registerInputDevice(qtdev);

        if (qtdev->type() == QInputDevice::DeviceType::Mouse || qtdev->type() == QInputDevice::DeviceType::TouchPad) {
            auto primaryQtDevice = QPointingDevice::primaryPointingDevice();
            if (!WInputDevice::from(primaryQtDevice)) {
                // Ensure the primary pointing device is the WInputDevice
                auto pd = const_cast<QPointingDevice*>(primaryQtDevice);
                pd->setParent(nullptr);
                delete pd;
            }
            Q_ASSERT(WInputDevice::from(QPointingDevice::primaryPointingDevice()));
        } else if (qtdev->type() == QInputDevice::DeviceType::Keyboard) {
            auto primaryQtDevice = QInputDevice::primaryKeyboard();
            if (!WInputDevice::from(primaryQtDevice)) {
                // Ensure the primary keyboard device is the WInputDevice
                auto pd = const_cast<QInputDevice*>(primaryQtDevice);
                pd->setParent(nullptr);
                delete pd;
            }
            Q_ASSERT(WInputDevice::from(QInputDevice::primaryKeyboard()));
        }
    }

    return qtdev;
}

bool QWlrootsIntegration::removeInputDevice(WInputDevice *device)
{
    if (auto qdevice = getInputDeviceFrom(device)) {
        delete qdevice;
        return true;
    }

    return false;
}

QInputDevice *QWlrootsIntegration::getInputDeviceFrom(WInputDevice *device)
{
    return device->qtDevice();
}

void QWlrootsIntegration::initialize()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    m_services.reset(new QDesktopUnixServices);
#else
    m_services.reset(new QGenericUnixServices);
#endif

    if (m_onInitialized)
        m_onInitialized();

    if (!qGuiApp->primaryScreen()) {
        m_placeholderScreen.reset(new QPlatformPlaceholderScreen);
        QWindowSystemInterface::handleScreenAdded(m_placeholderScreen.get(), true);
    }
}

void QWlrootsIntegration::destroy()
{
    if (m_placeholderScreen)
        QWindowSystemInterface::handleScreenRemoved(m_placeholderScreen.release());
}

bool QWlrootsIntegration::hasCapability(Capability cap) const
{
    switch (cap) {
    case OpenGL:
    case ThreadedOpenGL:
    case BufferQueueingOpenGL:
    case ThreadedPixmaps:
    case WindowMasks:
    case MultipleWindows:
    case ApplicationState:
    case ForeignWindows:
    case NonFullScreenWindows:
    case WindowManagement:
    case WindowActivation:
    case SyncState:
#if QT_VERSION < QT_VERSION_CHECK(6, 11, 0)
    case RasterGLSurface:
#endif
    case AllGLFunctionsQueryable:
    case ApplicationIcon:
    case SwitchableWidgetComposition:
    case OpenGLOnRasterSurface:
    case MaximizeUsingFullscreenGeometry:
    case PaintEvents:
    case RhiBasedRendering:
    case ScreenWindowGrabbing:
        return true;
    default: return false;
    }
}

QPlatformFontDatabase *QWlrootsIntegration::fontDatabase() const
{
    if (!m_fontDb)
        m_fontDb.reset(new QGenericUnixFontDatabase);
    return m_fontDb.get();
}

QPlatformWindow *QWlrootsIntegration::createPlatformWindow(QWindow *window) const
{
    if (QW::Window::check(window)) {
        Q_ASSERT(window->screen() && (window->screen()->handle() == m_placeholderScreen.get()
                                      || dynamic_cast<QWlrootsScreen*>(window->screen()->handle())));
        return new QWlrootsOutputWindow(window);
    } else if (QW::RenderWindow::check(window)) {
        return new QWlrootsRenderWindow(window);
    }

    return nullptr;
}

QPlatformBackingStore *QWlrootsIntegration::createPlatformBackingStore([[maybe_unused]] QWindow *window) const
{
    return nullptr;
}

#ifndef QT_NO_OPENGL
class Q_DECL_HIDDEN OpenGLContext : public QPlatformOpenGLContext {
public:
    explicit OpenGLContext(QOpenGLContext *context)
        : m_context(context)
    {}

    void initialize() override {
        m_format.setBlueBufferSize(8);
        m_format.setRedBufferSize(8);
        m_format.setGreenBufferSize(8);
        m_format.setDepthBufferSize(8);
        m_format.setStencilBufferSize(8);
        m_format.setRenderableType(QSurfaceFormat::OpenGLES);
        m_format.setSwapBehavior(QSurfaceFormat::SingleBuffer);
        m_format.setMajorVersion(3);

        if (auto c = qobject_cast<QW::OpenGLContext*>(m_context)) {
            auto eglConfig = q_configFromGLFormat(c->eglDisplay(), m_format, false, EGL_PBUFFER_BIT);
            if (eglConfig)
                m_format = q_glFormatFromConfig(c->eglDisplay(), eglConfig, m_format);
            Q_ASSERT(m_format.renderableType() == QSurfaceFormat::OpenGLES);
        } else {
            qWarning() << "The QSurfaceFormat is not initialize";
        }
    }

    QSurfaceFormat format() const override {
        return m_format;
    }

    void swapBuffers([[maybe_unused]] QPlatformSurface *surface) override {
        Q_UNREACHABLE();
    }
    GLuint defaultFramebufferObject([[maybe_unused]] QPlatformSurface *surface) const override {
        return 0;
    }

    bool makeCurrent([[maybe_unused]] QPlatformSurface *surface) override {
        if (auto c = qobject_cast<QW::OpenGLContext*>(m_context)) {
            return eglMakeCurrent(c->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, c->eglContext());
        }

        return false;
    }
    void doneCurrent() override {
        if (auto c = qobject_cast<QW::OpenGLContext*>(m_context)) {
            eglMakeCurrent(c->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
    }

    QFunctionPointer getProcAddress(const char *procName) override {
        return eglGetProcAddress(procName);
    }

private:
    QOpenGLContext *m_context = nullptr;
    QSurfaceFormat m_format;
};

QPlatformOpenGLContext *QWlrootsIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    if (QW::OpenGLContext::check(context))
        return new OpenGLContext(context);

    return QPlatformIntegration::createPlatformOpenGLContext(context);
}

QOpenGLContext::OpenGLModuleType QWlrootsIntegration::openGLModuleType()
{
    return QOpenGLContext::LibGLES;
}
#endif

QAbstractEventDispatcher *QWlrootsIntegration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}

QPlatformNativeInterface *QWlrootsIntegration::nativeInterface() const
{
    return const_cast<QWlrootsIntegration*>(this);
}

QPlatformPixmap *QWlrootsIntegration::createPlatformPixmap(QPlatformPixmap::PixelType type) const
{
    return QPlatformIntegration::createPlatformPixmap(type);
}

QPlatformWindow *QWlrootsIntegration::createForeignWindow(QWindow *window, WId id) const
{
    return QPlatformIntegration::createForeignWindow(window, id);
}

QPlatformSharedGraphicsCache *QWlrootsIntegration::createPlatformSharedGraphicsCache(const char *cacheId) const
{
    return QPlatformIntegration::createPlatformSharedGraphicsCache(cacheId);
}

QPaintEngine *QWlrootsIntegration::createImagePaintEngine(QPaintDevice *paintDevice) const
{
    return QPlatformIntegration::createImagePaintEngine(paintDevice);
}

#ifndef QT_NO_CLIPBOARD
QPlatformClipboard *QWlrootsIntegration::clipboard() const
{
    return QPlatformIntegration::clipboard();
}
#endif

#if QT_CONFIG(draganddrop)
QPlatformDrag *QWlrootsIntegration::drag() const
{
    return QPlatformIntegration::drag();
}
#endif

QPlatformInputContext *QWlrootsIntegration::inputContext() const
{
    return QPlatformIntegration::inputContext();
}

#if QT_CONFIG(accessibility)
QPlatformAccessibility *QWlrootsIntegration::accessibility() const
{
    return nullptr;
}
#endif

QPlatformServices *QWlrootsIntegration::services() const
{
    return m_services.get();
}

QVariant QWlrootsIntegration::styleHint(StyleHint hint) const
{
    return QPlatformIntegration::styleHint(hint);
}

Qt::WindowState QWlrootsIntegration::defaultWindowState(Qt::WindowFlags flags) const
{
    return QPlatformIntegration::defaultWindowState(flags);
}

QStringList QWlrootsIntegration::themeNames() const
{
    return QPlatformIntegration::themeNames();
}

QPlatformTheme *QWlrootsIntegration::createPlatformTheme([[maybe_unused]] const QString &name) const
{
    // TODO: use custom platform theme to allow compositor set some hints, like as font
    return new QGenericUnixTheme();
}

QPlatformOffscreenSurface *QWlrootsIntegration::createPlatformOffscreenSurface(QOffscreenSurface *surface) const
{
    if (QW::OffscreenSurface::check(surface))
        return new PlatformOffscreenSurface(surface);

    return QPlatformIntegration::createPlatformOffscreenSurface(surface);
}

#ifndef QT_NO_SESSIONMANAGER
QPlatformSessionManager *QWlrootsIntegration::createPlatformSessionManager(const QString &id, const QString &key) const
{
    return QPlatformIntegration::createPlatformSessionManager(id, key);
}
#endif

void QWlrootsIntegration::sync()
{
    QPlatformIntegration::sync();
}

void QWlrootsIntegration::setApplicationIcon(const QIcon &icon) const
{
    QPlatformIntegration::setApplicationIcon(icon);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
void QWlrootsIntegration::setApplicationBadge(qint64 number)
{
    QPlatformIntegration::setApplicationBadge(number);
}
#endif

void QWlrootsIntegration::beep() const
{
    QPlatformIntegration::beep();
}

void QWlrootsIntegration::quit() const
{
    QPlatformIntegration::quit();
}

void *QWlrootsIntegration::nativeResourceForScreen(const QByteArray &resource, QScreen *screen)
{
    if (resource == QByteArrayView("antialiasingEnabled")) {
        return reinterpret_cast<void*>(0x1);
    }

    return QPlatformNativeInterface::nativeResourceForScreen(resource, screen);
}

#if QT_CONFIG(vulkan)
class Q_DECL_HIDDEN VulkanInstance : public QBasicPlatformVulkanInstance
{
public:
    VulkanInstance(QVulkanInstance *instance)
        : m_instance(instance)
    {
        loadVulkanLibrary(QStringLiteral("vulkan"), 1);
    }

    void createOrAdoptInstance() override {
        initInstance(m_instance, {});
    }

private:
    QVulkanInstance *m_instance;
};

QPlatformVulkanInstance *QWlrootsIntegration::createPlatformVulkanInstance(QVulkanInstance *instance) const
{
    return new VulkanInstance(instance);
}
#endif

WAYLIB_SERVER_END_NAMESPACE
