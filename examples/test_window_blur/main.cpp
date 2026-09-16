// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// This demo is modeled after quickshell's background_effect implementation:
// https://github.com/quickshell-mirror/quickshell/tree/master/src/wayland/background_effect
// (surface lifecycle handling, capability checks, HiDPI region scaling and
// client-side margin handling follow the same approach used there.)

#include "qwayland-ext-background-effect-v1.h"

#include <private/qhighdpiscaling_p.h>
#include <private/qwaylanddisplay_p.h>
#include <private/qwaylandintegration_p.h>
#include <private/qwaylandwindow_p.h>

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWindow>
#include <QtWaylandClient/QWaylandClientExtension>

#include <memory>

using QtWaylandClient::QWaylandWindow;

class BackgroundEffectManager
    : public QWaylandClientExtensionTemplate<BackgroundEffectManager>
    , public QtWayland::ext_background_effect_manager_v1
{
    Q_OBJECT
public:
    explicit BackgroundEffectManager()
        : QWaylandClientExtensionTemplate<BackgroundEffectManager>(1)
    {
        initialize();
    }

    bool blurAvailable() const { return isActive() && m_blurAvailable; }

protected:
    void ext_background_effect_manager_v1_capabilities(uint32_t flags) override
    {
        m_blurAvailable = flags & capability_blur;
    }

private:
    bool m_blurAvailable = false;
};

class BackgroundEffectSurface : public QtWayland::ext_background_effect_surface_v1
{
public:
    explicit BackgroundEffectSurface(struct ::ext_background_effect_surface_v1 *surface)
        : QtWayland::ext_background_effect_surface_v1(surface)
    {
    }

    ~BackgroundEffectSurface() override
    {
        if (isInitialized())
            destroy();
    }

    void setBlurRegion(const QRegion &region)
    {
        if (!isInitialized())
            return;

        if (region.isEmpty()) {
            // NULL region removes the effect.
            set_blur_region(nullptr);
            return;
        }

        auto *display = QtWaylandClient::QWaylandIntegration::instance()->display();
        struct ::wl_region *wlRegion = display->createRegion(region);
        set_blur_region(wlRegion);
        // set_blur_region has copy semantics, safe to destroy immediately.
        wl_region_destroy(wlRegion);
    }
};

class BlurWindow : public QWidget
{
    Q_OBJECT
public:
    explicit BlurWindow()
    {
        setWindowTitle("Window Blur Test");
        // The window itself must be translucent so that the background
        // behind it can be seen and blurred.
        setAttribute(Qt::WA_TranslucentBackground);
        resize(640, 480);

        auto *layout = new QVBoxLayout(this);

        m_statusLabel = new QLabel("Blur region: off", this);
        layout->addWidget(m_statusLabel);

        auto *toggleButton = new QPushButton("Toggle blur region", this);
        layout->addWidget(toggleButton);
        connect(toggleButton, &QPushButton::clicked, this, &BlurWindow::toggleBlur);
    }

protected:
    // The platform surface (and with it the wl_surface) may be created after
    // showEvent, so the event filter is the earliest reliable hook. When the
    // window is hidden the wl_surface is destroyed; keep the effect in sync
    // with that lifecycle so it is re-created on the next show.
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::PlatformSurface) {
            auto *surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
            if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
                attachWaylandWindow();
            } else if (surfaceEvent->surfaceEventType()
                       == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
                detachWaylandWindow();
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        if (!windowHandle()->parent())
            windowHandle()->installEventFilter(this);
        // The platform surface may already exist when re-showing.
        attachWaylandWindow();
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        // The blur region is surface-local state; keep it in sync with the
        // window size so it always covers the whole surface.
        applyBlurRegion();
    }

private:
    void toggleBlur()
    {
        m_blurEnabled = !m_blurEnabled;
        createEffectSurface();
        applyBlurRegion();
        m_statusLabel->setText(m_blurEnabled ? "Blur region: on" : "Blur region: off");
    }

    void attachWaylandWindow()
    {
        auto *waylandWindow = dynamic_cast<QWaylandWindow *>(windowHandle()->handle());
        if (waylandWindow == m_waylandWindow)
            return;

        if (m_waylandWindow) {
            disconnect(m_waylandWindow, nullptr, this, nullptr);
            m_effect.reset();
        }

        m_waylandWindow = waylandWindow;
        if (!m_waylandWindow)
            return;

        connect(m_waylandWindow,
                &QWaylandWindow::surfaceCreated,
                this,
                &BlurWindow::createEffectSurface);
        connect(m_waylandWindow,
                &QWaylandWindow::surfaceDestroyed,
                this,
                &BlurWindow::onWaylandSurfaceDestroyed);

        // The wl_surface may already exist when the filter installs late.
        if (m_waylandWindow->surface())
            createEffectSurface();
    }

    void detachWaylandWindow()
    {
        if (m_waylandWindow) {
            disconnect(m_waylandWindow, nullptr, this, nullptr);
            m_waylandWindow = nullptr;
        }
        m_effect.reset();
    }

    void createEffectSurface()
    {
        if (m_effect || !m_blurEnabled || !m_waylandWindow)
            return;

        if (!m_manager.blurAvailable()) {
            qWarning() << "ext-background-effect-v1 blur is not supported by the compositor";
            return;
        }

        if (!m_waylandWindow->surface())
            return;

        m_effect = std::make_unique<BackgroundEffectSurface>(
            m_manager.get_background_effect(m_waylandWindow->surface()));
        applyBlurRegion();
    }

    void onWaylandSurfaceDestroyed()
    {
        m_effect.reset();
    }

    void applyBlurRegion()
    {
        if (!m_effect)
            return;

        QRegion region;
        if (m_blurEnabled) {
            region = QRegion(0, 0, width(), height());

            // Blur region is specified in surface-local (native) coordinates:
            // scale logical pixels by the device pixel ratio and offset by the
            // client-side frame margins, mirroring quickshell's handling.
            const auto scale = QHighDpiScaling::factor(windowHandle());
            if (!qFuzzyCompare(scale, 1.0))
                region = QHighDpi::scale(region, scale);

            const auto margins = m_waylandWindow->clientSideMargins();
            region.translate(margins.left(), margins.top());
        }

        m_effect->setBlurRegion(region);
    }

    BackgroundEffectManager m_manager;
    QLabel *m_statusLabel = nullptr;
    QWaylandWindow *m_waylandWindow = nullptr;
    std::unique_ptr<BackgroundEffectSurface> m_effect;
    bool m_blurEnabled = false;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    BlurWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
