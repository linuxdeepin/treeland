// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwlrootsintegration.h"
#include "qwlrootscreen.h"
#include "qwlrootswindow.h"
#include "woutput.h"
#include "winputdevice.h"

#define private public
#include <QScreen>
#undef private

#include <qwoutput.h>
#include <qwrenderer.h>
#include <qwinputdevice.h>
#include <qwdisplay.h>
#include <QThread>
#include <QGuiApplication>

#include <private/qgenericunixfontdatabase_p.h>
#include <private/qgenericunixservices_p.h>
#include <private/qgenericunixeventdispatcher_p.h>
#if QT_CONFIG(vulkan)
#include <private/qvulkaninstance_p.h>
#include <private/qbasicvulkanplatforminstance_p.h>
#endif
#include <private/qinputdevice_p.h>
#include <qpa/qplatformsurface.h>
#include <qpa/qwindowsysteminterface.h>

#ifndef QT_NO_OPENGL
#include <qpa/qplatformopenglcontext.h>
#include <private/qeglconvenience_p.h>
#include <EGL/egl.h>

extern "C" {
#include <wlr/render/egl.h>
#define static
#include <wlr/render/gles2.h>
}

#endif

extern "C" {
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_tablet_pad.h>
}

WAYLIB_SERVER_BEGIN_NAMESPACE

#define CALL_PROXY2(FunName, fallbackValue, ...) m_proxyIntegration ? m_proxyIntegration->FunName(__VA_ARGS__) : fallbackValue
#define CALL_PROXY(FunName, ...) CALL_PROXY2(FunName, QPlatformIntegration::FunName(__VA_ARGS__), __VA_ARGS__)

QWlrootsIntegration *QWlrootsIntegration::m_instance = nullptr;
QWlrootsIntegration::QWlrootsIntegration(bool master, const QStringList &parameters, std::function<void ()> onInitialized)
    : m_master(master)
    , m_onInitialized(onInitialized)
{
    Q_UNUSED(parameters);
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

void QWlrootsIntegration::setProxy(QPlatformIntegration *proxy)
{
    Q_ASSERT(!m_proxyIntegration);
    Q_ASSERT_X(!m_master, "initializeProxyQPA", "Can't set proxy plugin for a master QPA plugin.");
    m_proxyIntegration.reset(proxy);
    if (m_placeholderScreen) {
        QWindowSystemInterface::handleScreenRemoved(m_placeholderScreen.release());
    }
    m_fontDb.reset();
}

QWlrootsScreen *QWlrootsIntegration::addScreen(WOutput *output)
{
    m_screens << new QWlrootsScreen(output);

    if (isMaster()) {
        QWindowSystemInterface::handleScreenAdded(m_screens.last(), output);

        if (m_placeholderScreen) {
            QWindowSystemInterface::handleScreenRemoved(m_placeholderScreen.release());
        }
    } else {
        Q_UNUSED(new QScreen(m_screens.last()));
    }

    output->setScreen(m_screens.last());

    return m_screens.last();
}

void QWlrootsIntegration::removeScreen(WOutput *output)
{
    if (auto screen = output->screen()) {
        output->setScreen(nullptr);
        bool ok = m_screens.removeOne(screen);

        if (!isMaster()) {
            delete screen->screen();
            delete screen;
            return;
        }

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

QInputDevice *QWlrootsIntegration::addInputDevice(WInputDevice *device, const QString &seatName)
{
    QInputDevice *qtdev = nullptr;
    auto qwDevice = device->nativeInterface<QWInputDevice>();
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
    case WLR_INPUT_DEVICE_TABLET_TOOL: {
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
    }

    return qtdev;
}

void QWlrootsIntegration::removeInputDevice(WInputDevice *device)
{
    if (auto qdevice = getInputDeviceFrom(device)) {
        device->setQtDevice(nullptr);
        qdevice->deleteLater();
    }
}

QInputDevice *QWlrootsIntegration::getInputDeviceFrom(WInputDevice *device)
{
    return device->qtDevice();
}

void QWlrootsIntegration::initialize()
{
    if (isMaster()) {
        m_services.reset(new QGenericUnixServices);
    }

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

    if (m_proxyIntegration)
        m_proxyIntegration->destroy();
}

bool QWlrootsIntegration::hasCapability(Capability cap) const
{
    if (!isMasterThread()) {
        return CALL_PROXY(hasCapability, cap);
    }

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
    case RasterGLSurface:
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
    if (m_proxyIntegration)
        return m_proxyIntegration->fontDatabase();

    if (!m_fontDb)
        m_fontDb.reset(new QGenericUnixFontDatabase);
    return m_fontDb.get();
}

QPlatformWindow *QWlrootsIntegration::createPlatformWindow(QWindow *window) const
{
    if (window->objectName() == QStringLiteral(QT_STRINGIFY(WAYLIB_SERVER_NAMESPACE))) {
        Q_ASSERT(window->screen() && dynamic_cast<QWlrootsScreen*>(window->screen()->handle()));
        return new QWlrootsOutputWindow(window);
    }

    if (isMasterThread())
        return nullptr;

    return CALL_PROXY2(createPlatformWindow, nullptr, window);
}

QPlatformBackingStore *QWlrootsIntegration::createPlatformBackingStore(QWindow *window) const
{
    Q_ASSERT(!isMasterThread());
    return CALL_PROXY2(createPlatformBackingStore, nullptr, window);
}

static inline QWOutput *outputFrom(QPlatformSurface *surface) {
    auto ws = dynamic_cast<QWlrootsScreen*>(surface->screen());
    return ws ? ws->output()->nativeInterface<QWOutput>() : nullptr;
}

#ifndef QT_NO_OPENGL
class OpenGLContext : public QPlatformOpenGLContext {
public:
    explicit OpenGLContext(QOpenGLContext *context)
        : m_context(context)
    {}

    inline QWlrootsScreen *screen() const {
        if (!m_context->screen())
            return nullptr;
        auto s = dynamic_cast<QWlrootsScreen*>(m_context->screen()->handle());
        Q_ASSERT(s);
        return s;
    }
    inline QWOutput *output() const {
        auto ws = screen();
        return ws ? ws->output()->nativeInterface<QWOutput>() : nullptr;
    }
    inline QWRenderer *renderer() const {
        auto output = this->output();
        return output ? QWRenderer::from(output->handle()->renderer) : nullptr;
    }

    void initialize() override {
        m_format.setRenderableType(QSurfaceFormat::OpenGLES);

        auto renderer = this->renderer();
        if (!renderer)
            return;

        if (!wlr_renderer_is_gles2(renderer->handle()))
            return;

        auto egl = wlr_gles2_renderer_get_egl(renderer->handle());
#if WLR_VERSION_MINOR > 15
        auto display = wlr_egl_get_display(egl);
#else
        auto display = egl->display;
#endif
        auto eglConfig = q_configFromGLFormat(display, m_format, false, EGL_WINDOW_BIT);
        if (!eglConfig)
            m_format = q_glFormatFromConfig(display, eglConfig, m_format);
        Q_ASSERT(m_format.renderableType() == QSurfaceFormat::OpenGLES);
    }

    QSurfaceFormat format() const override {
        return m_format;
    }

    void swapBuffers(QPlatformSurface *surface) override {
        if (QWOutput *output = outputFrom(surface))
            output->commit();
    }
    GLuint defaultFramebufferObject(QPlatformSurface *surface) const override {
        if (QWOutput *output = outputFrom(surface)) {
            return wlr_gles2_renderer_get_current_fbo(output->handle()->renderer);
        }

        return 0;
    }

    bool makeCurrent(QPlatformSurface *surface) override {
        if (QWOutput *output = outputFrom(surface)) {
            if (!wlr_renderer_is_gles2(output->handle()->renderer))
                return false;

            return output->attachRender(nullptr);
        }

        return false;
    }
    void doneCurrent() override {
        if (QWOutput *output = this->output()) {
            return output->rollback();
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
    if (context->objectName() == QStringLiteral(QT_STRINGIFY(WAYLIB_SERVER_NAMESPACE)))
        return new OpenGLContext(context);

    if (isMasterThread())
        return nullptr;

    return CALL_PROXY(createPlatformOpenGLContext, context);
}

QOpenGLContext::OpenGLModuleType QWlrootsIntegration::openGLModuleType()
{
    return QOpenGLContext::LibGLES;
}
#endif

QAbstractEventDispatcher *QWlrootsIntegration::createEventDispatcher() const
{
    return CALL_PROXY2(createEventDispatcher, createUnixEventDispatcher());
}

QPlatformNativeInterface *QWlrootsIntegration::nativeInterface() const
{
    if (isMasterThread())
        return nullptr;

    return CALL_PROXY(nativeInterface);
}

QPlatformPixmap *QWlrootsIntegration::createPlatformPixmap(QPlatformPixmap::PixelType type) const
{
    if (isMasterThread())
        return QPlatformIntegration::createPlatformPixmap(type);

    return CALL_PROXY(createPlatformPixmap, type);
}

QPlatformWindow *QWlrootsIntegration::createForeignWindow(QWindow *window, WId id) const
{
    Q_ASSERT(!isMasterThread());
    return CALL_PROXY(createForeignWindow, window, id);
}

QPlatformSharedGraphicsCache *QWlrootsIntegration::createPlatformSharedGraphicsCache(const char *cacheId) const
{
    Q_ASSERT(!isMasterThread());
    return CALL_PROXY(createPlatformSharedGraphicsCache, cacheId);
}

QPaintEngine *QWlrootsIntegration::createImagePaintEngine(QPaintDevice *paintDevice) const
{
    if (isMasterThread())
        return QPlatformIntegration::createImagePaintEngine(paintDevice);

    return CALL_PROXY(createImagePaintEngine, paintDevice);
}

#ifndef QT_NO_CLIPBOARD
QPlatformClipboard *QWlrootsIntegration::clipboard() const
{
    Q_ASSERT(!isMasterThread());
    return CALL_PROXY(clipboard);
}
#endif

#if QT_CONFIG(draganddrop)
QPlatformDrag *QWlrootsIntegration::drag() const
{
    Q_ASSERT(!isMasterThread());
    return CALL_PROXY(drag);
}
#endif

QPlatformInputContext *QWlrootsIntegration::inputContext() const
{
    if (isMasterThread())
        return nullptr;
    return CALL_PROXY(inputContext);
}

#if QT_CONFIG(accessibility)
QPlatformAccessibility *QWlrootsIntegration::accessibility() const
{
    return nullptr;
}
#endif

QPlatformServices *QWlrootsIntegration::services() const
{
    if (isMaster())
        return m_services.get();

    return CALL_PROXY(services);
}

QVariant QWlrootsIntegration::styleHint(StyleHint hint) const
{
    return CALL_PROXY(styleHint, hint);
}

Qt::WindowState QWlrootsIntegration::defaultWindowState(Qt::WindowFlags flags) const
{
    if (isMasterThread())
        return Qt::WindowFullScreen;

    return CALL_PROXY(defaultWindowState, flags);
}

Qt::KeyboardModifiers QWlrootsIntegration::queryKeyboardModifiers() const
{
    Q_ASSERT(!isMasterThread());
    return CALL_PROXY(queryKeyboardModifiers);
}

QList<int> QWlrootsIntegration::possibleKeys(const QKeyEvent *event) const
{
    Q_ASSERT(!isMasterThread());
    return CALL_PROXY(possibleKeys, event);
}

QStringList QWlrootsIntegration::themeNames() const
{
    Q_ASSERT(!isMasterThread());
    return CALL_PROXY(themeNames);
}

QPlatformTheme *QWlrootsIntegration::createPlatformTheme(const QString &name) const
{
    Q_ASSERT(!isMasterThread());
    return CALL_PROXY(createPlatformTheme, name);
}

QPlatformOffscreenSurface *QWlrootsIntegration::createPlatformOffscreenSurface(QOffscreenSurface *surface) const
{
    Q_ASSERT(!isMasterThread());
    return CALL_PROXY(createPlatformOffscreenSurface, surface);
}

#ifndef QT_NO_SESSIONMANAGER
QPlatformSessionManager *QWlrootsIntegration::createPlatformSessionManager(const QString &id, const QString &key) const
{
    Q_ASSERT(!isMasterThread());
    return CALL_PROXY(createPlatformSessionManager, id, key);
}
#endif

void QWlrootsIntegration::sync()
{
    Q_ASSERT(!isMasterThread());
    CALL_PROXY(sync);
}

void QWlrootsIntegration::setApplicationIcon(const QIcon &icon) const
{
    Q_ASSERT(!isMasterThread());
    CALL_PROXY(setApplicationIcon, icon);
}

void QWlrootsIntegration::setApplicationBadge(qint64 number)
{
    Q_ASSERT(!isMasterThread());
    CALL_PROXY(setApplicationBadge, number);
}

void QWlrootsIntegration::beep() const
{
    Q_ASSERT(!isMasterThread());
    CALL_PROXY(beep);
}

void QWlrootsIntegration::quit() const
{
    Q_ASSERT(!isMasterThread());
    CALL_PROXY(quit);
}

#if QT_CONFIG(vulkan)
class VulkanInstance : public QBasicPlatformVulkanInstance
{
public:
    VulkanInstance(QVulkanInstance *instance)
        : m_instance(instance)
    {
        loadVulkanLibrary(QStringLiteral("vulkan"));
    }

    void createOrAdoptInstance() override {
        initInstance(m_instance, {});
    }

private:
    QVulkanInstance *m_instance;
};

QPlatformVulkanInstance *QWlrootsIntegration::createPlatformVulkanInstance(QVulkanInstance *instance) const
{
    if (isMasterThread())
        return new VulkanInstance(instance);

    return m_proxyIntegration ? m_proxyIntegration->createPlatformVulkanInstance(instance) : nullptr;
}
#endif

WAYLIB_SERVER_END_NAMESPACE
