// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// This demo is modeled after quickshell's background_effect implementation:
// https://github.com/quickshell-mirror/quickshell/tree/master/src/wayland/background_effect
// (surface lifecycle handling, capability checks, and blur region management
// follow the same approach used there.)

#include "qwayland-ext-background-effect-v1.h"

#include <private/qwaylanddisplay_p.h>
#include <private/qwaylandwindow_p.h>

#include <QApplication>
#include <QDebug>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QtWaylandClient/QWaylandClientExtension>

// Modeled after quickshell's qs::wayland::background_effect implementation:
// https://github.com/quickshell-mirror/quickshell/tree/master/src/wayland/background_effect
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

class BlurWindow : public QWidget
{
    Q_OBJECT
public:
    explicit BlurWindow()
        : m_manager(new BackgroundEffectManager)
    {
        setWindowTitle("Window Blur Test");
        // The window itself must be translucent so that the background
        // behind it can be seen and blurred.
        setAttribute(Qt::WA_TranslucentBackground);
        resize(640, 480);

        auto *layout = new QVBoxLayout(this);

        auto *statusLabel = new QLabel("Blur region: off", this);
        layout->addWidget(statusLabel);

        auto *toggleButton = new QPushButton("Toggle blur region", this);
        layout->addWidget(toggleButton);
        connect(toggleButton, &QPushButton::clicked, this, [this, statusLabel] {
            m_blurEnabled = !m_blurEnabled;
            applyBlurRegion();
            statusLabel->setText(m_blurEnabled ? "Blur region: on" : "Blur region: off");
        });
    }

    ~BlurWindow() override
    {
        // m_effect is destroyed in eventFilter() on SurfaceAboutToBeDestroyed,
        // which Qt delivers before the underlying wl_surface goes away.
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::PlatformSurface) {
            auto *surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
            switch (surfaceEvent->surfaceEventType()) {
            case QPlatformSurfaceEvent::SurfaceCreated:
                createEffectSurface();
                break;
            case QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed:
                // Destroy the ext_background_effect_surface_v1 before the
                // wl_surface, otherwise the compositor raises the
                // surface_destroyed protocol error.
                destroyEffectSurface();
                break;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        if (QWindow *window = windowHandle()) {
            if (!window->parent()) {
                window->installEventFilter(this);
            }
            // The platform surface may already be created at this point.
            createEffectSurface();
        }
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        // The blur region is surface-local state; keep it in sync with the
        // window size so it always covers the whole surface.
        applyBlurRegion();
    }

private:
    QtWaylandClient::QWaylandWindow *waylandWindow() const
    {
        QWindow *window = windowHandle();
        if (!window || !window->handle())
            return nullptr;
        return static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
    }

    void createEffectSurface()
    {
        if (m_effect)
            return;

        auto *waylandWindow = this->waylandWindow();
        if (!waylandWindow || !waylandWindow->surface())
            return;

        if (!m_manager->blurAvailable()) {
            qWarning() << "ext-background-effect-v1 blur is not supported by the compositor";
            return;
        }

        m_effect = new QtWayland::ext_background_effect_surface_v1(
            m_manager->get_background_effect(waylandWindow->surface()));
        applyBlurRegion();
    }

    void destroyEffectSurface()
    {
        if (!m_effect)
            return;

        if (m_effect->isInitialized())
            m_effect->destroy();
        delete m_effect;
        m_effect = nullptr;
    }

    void applyBlurRegion()
    {
        if (!m_effect || !m_effect->isInitialized())
            return;

        if (!m_blurEnabled) {
            // NULL region removes the effect.
            m_effect->set_blur_region(nullptr);
            return;
        }

        auto *waylandWindow = this->waylandWindow();
        auto *display = waylandWindow ? waylandWindow->display() : nullptr;
        if (!display)
            return;

        // Full-surface blur, in surface-local (logical) coordinates; the
        // compositor clips the region to the surface size.
        QRegion region(0, 0, width(), height());

        struct ::wl_region *wlRegion = display->createRegion(region);
        m_effect->set_blur_region(wlRegion);
        // set_blur_region has copy semantics, safe to destroy immediately.
        wl_region_destroy(wlRegion);
    }

    BackgroundEffectManager *m_manager = nullptr;
    QtWayland::ext_background_effect_surface_v1 *m_effect = nullptr;
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
