// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-treeland-window-transition-unstable-v1.h"
#include "qwayland-xdg-activation-v1.h"

#include <private/qwaylanddisplay_p.h>
#include <private/qwaylandshmbackingstore_p.h>
#include <private/qwaylandwindow_p.h>

#include <QApplication>
#include <QDebug>
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QPainter>
#include <QProcess>
#include <QProcessEnvironment>
#include <QResizeEvent>
#include <QScreen>
#include <QVBoxLayout>
#include <QWidget>
#include <QtWaylandClient/QWaylandClientExtension>

// ---------------------------------------------------------------------------
// Protocol client wrappers — managers (globals)
// ---------------------------------------------------------------------------

class XdgActivationManager
    : public QWaylandClientExtensionTemplate<XdgActivationManager>
    , public QtWayland::xdg_activation_v1
{
    Q_OBJECT
public:
    explicit XdgActivationManager()
        : QWaylandClientExtensionTemplate<XdgActivationManager>(1)
    {
    }

    ~XdgActivationManager() override
    {
        if (isInitialized())
            destroy();
    }
};

class WindowTransitionManager
    : public QWaylandClientExtensionTemplate<WindowTransitionManager>
    , public QtWayland::treeland_window_transition_manager_v1
{
    Q_OBJECT
public:
    explicit WindowTransitionManager()
        : QWaylandClientExtensionTemplate<WindowTransitionManager>(1)
    {
    }

    ~WindowTransitionManager() override
    {
        if (isInitialized())
            destroy();
    }
};

// ---------------------------------------------------------------------------
// Per-object wrappers
// ---------------------------------------------------------------------------

class ActivationToken
    : public QObject
    , public QtWayland::xdg_activation_token_v1
{
    Q_OBJECT
public:
    explicit ActivationToken(::xdg_activation_token_v1 *obj, QObject *parent = nullptr)
        : QObject(parent)
        , QtWayland::xdg_activation_token_v1(obj)
    {
    }

    QString tokenString() const
    {
        return m_token;
    }

protected:
    void xdg_activation_token_v1_done(const QString &token) override
    {
        m_token = token;
        Q_EMIT done(token);
    }

Q_SIGNALS:
    void done(const QString &token);

private:
    QString m_token;
};

class WindowTransitionRect
    : public QObject
    , public QtWayland::treeland_window_transition_rect_v1
{
    Q_OBJECT
public:
    explicit WindowTransitionRect(::treeland_window_transition_rect_v1 *obj,
                                  QObject *parent = nullptr)
        : QObject(parent)
        , QtWayland::treeland_window_transition_rect_v1(obj)
    {
    }

    void setSourceImage(QtWaylandClient::QWaylandDisplay *display, const QPixmap &pixmap)
    {
        delete m_shmBuffer;
        m_shmBuffer = new QtWaylandClient::QWaylandShmBuffer(display,
                                                             pixmap.size(),
                                                             QImage::Format_ARGB32_Premultiplied);
        QImage *image = m_shmBuffer->image();
        image->fill(Qt::transparent);
        QPainter p(image);
        p.drawPixmap(0, 0, pixmap);
        p.end();
        set_source_buffer(m_shmBuffer->buffer());
    }

    void clearSourceImage()
    {
        delete m_shmBuffer;
        m_shmBuffer = nullptr;
        set_source_buffer(nullptr);
    }

    ~WindowTransitionRect() override
    {
        delete m_shmBuffer;
    }

protected:
    void treeland_window_transition_rect_v1_closed() override
    {
        Q_EMIT closed();
    }

Q_SIGNALS:
    void closed();

private:
    QtWaylandClient::QWaylandShmBuffer *m_shmBuffer = nullptr;
};

// ---------------------------------------------------------------------------
// Test image
// ---------------------------------------------------------------------------

static QPixmap createTestImage(int w, int h)
{
    QPixmap pm(w, h);
    QPainter p(&pm);
    QLinearGradient grad(0, 0, w, h);
    grad.setColorAt(0, QColor(63, 81, 181));
    grad.setColorAt(1, QColor(33, 150, 243));
    p.fillRect(0, 0, w, h, grad);
    p.setPen(QPen(QColor(255, 255, 255, 200), 4));
    p.drawRoundedRect(10, 10, w - 20, h - 20, 20, 20);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPointSize(28);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, "Window\nTransition");
    return pm;
}

// ---------------------------------------------------------------------------
// Image widget
// ---------------------------------------------------------------------------

class ImageWidget : public QWidget
{
public:
    explicit ImageWidget(const QPixmap &pixmap, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_pixmap(pixmap)
    {
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.drawPixmap(
            0,
            0,
            m_pixmap.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    }

private:
    QPixmap m_pixmap;
};

// ---------------------------------------------------------------------------
// Helper: get wl_surface from a widget
// ---------------------------------------------------------------------------

static ::wl_surface *getWlSurface(QWidget *widget)
{
    QWindow *handle = widget->windowHandle();
    if (!handle || !handle->handle())
        return nullptr;
    auto *waylandWindow = static_cast<QtWaylandClient::QWaylandWindow *>(handle->handle());
    return waylandWindow ? waylandWindow->surface() : nullptr;
}

// ---------------------------------------------------------------------------
// Helper: get the wayland display behind a widget
// ---------------------------------------------------------------------------

static QtWaylandClient::QWaylandDisplay *getWlDisplay(QWidget *widget)
{
    QWindow *handle = widget->windowHandle();
    if (!handle || !handle->handle())
        return nullptr;
    auto *waylandWindow = static_cast<QtWaylandClient::QWaylandWindow *>(handle->handle());
    return waylandWindow ? waylandWindow->display() : nullptr;
}

// ---------------------------------------------------------------------------
// Sender (Application A)
// ---------------------------------------------------------------------------

class SenderWindow : public QMainWindow
{
    Q_OBJECT
public:
    SenderWindow(XdgActivationManager *actMgr, WindowTransitionManager *transitionMgr)
        : m_actMgr(actMgr)
        , m_transitionMgr(transitionMgr)
        , m_image(createTestImage(200, 200))
    {
        setWindowTitle("Window Transition - Sender (press Space)");

        auto *central = new QWidget(this);
        auto *layout = new QVBoxLayout(central);

        m_imageWidget = new ImageWidget(m_image, central);
        m_imageWidget->setFixedSize(200, 200);
        layout->addWidget(m_imageWidget, 0, Qt::AlignCenter);

        auto *label = new QLabel("SPACE: launch receiver (new process)\n"
                                 "ENTER: launch modal (same process)\n"
                                 "0: toggle disable animation (hide image)\n"
                                 "B: toggle commit source buffer",
                                 central);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);

        m_sourceBufferIndicator = new QLabel("Commit source buffer: ON", central);
        m_sourceBufferIndicator->setAlignment(Qt::AlignCenter);
        layout->addWidget(m_sourceBufferIndicator);

        setCentralWidget(central);
        resize(400, 300);
    }

    ~SenderWindow() override
    {
        // Destroy all rect objects so the compositor falls back
        // to the default close animation once sender is gone.
        for (auto *rect : std::as_const(m_rects)) {
            rect->destroy();
        }
        m_rects.clear();
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Space)
            launchReceiver();
        else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            launchModal();
        else if (event->key() == Qt::Key_0)
            toggleAnimation();
        else if (event->key() == Qt::Key_B)
            toggleSourceBuffer();
        QMainWindow::keyPressEvent(event);
    }

    void resizeEvent(QResizeEvent *event) override
    {
        // The image widget is centered in the layout, so resizing the main
        // window moves it. Re-set the persistent rect(s) with the updated
        // geometry so the open/close animation follows the new position.
        updateRectGeometries();
        QMainWindow::resizeEvent(event);
    }

private:
    // Geometry (relative to this sender surface) of the image widget that is
    // the source of the window transition.
    QRect animationRect() const
    {
        const QPoint imagePos = m_imageWidget->mapTo(this, QPoint(0, 0));
        return QRect(imagePos, m_imageWidget->size());
    }

    void toggleAnimation()
    {
        m_animationDisabled = !m_animationDisabled;
        m_imageWidget->setVisible(!m_animationDisabled);
        updateRectGeometries();
    }

    void toggleSourceBuffer()
    {
        m_commitSourceBuffer = !m_commitSourceBuffer;
        if (m_sourceBufferIndicator)
            m_sourceBufferIndicator->setText(m_commitSourceBuffer ? "Commit source buffer: ON"
                                                                  : "Commit source buffer: OFF");

        auto *display = getWlDisplay(this);
        for (auto *rect : std::as_const(m_rects)) {
            if (m_commitSourceBuffer && display)
                rect->setSourceImage(display, m_image);
            else
                rect->clearSourceImage();
        }
    }

    void updateRectGeometries()
    {
        if (m_rects.isEmpty())
            return;
        // When disabled, keep setting a 0x0 rect so the compositor stays in
        // the disabled state; a resize must not re-enable it.
        const QRect rect = m_animationDisabled ? QRect() : animationRect();
        for (auto *r : std::as_const(m_rects)) {
            r->set_geometry(rect.x(), rect.y(), rect.width(), rect.height());
        }
    }

    ActivationToken *createLaunchToken()
    {
        if (!m_actMgr || !m_actMgr->isInitialized()) {
            qWarning() << "xdg_activation_v1 not available";
            return nullptr;
        }

        ::wl_surface *surface = getWlSurface(this);
        if (!surface) {
            qWarning() << "Cannot get wl_surface";
            return nullptr;
        }

        // 1. Create activation token
        auto *rawToken = m_actMgr->get_activation_token();
        if (!rawToken) {
            qWarning() << "Failed to create activation token";
            return nullptr;
        }
        auto *token = new ActivationToken(rawToken, this);

        // 2. Set the originating surface on the token
        token->set_surface(surface);

        // 3. Attach window transition rect (relative to this surface).
        //    The rect is persistent: it stays alive after set_geometry and is
        //    used for both the open and close animations. We keep a reference
        //    so the compositor can read the latest geometry when the target
        //    window closes. Destroying the rect (or disconnecting) makes the
        //    compositor fall back to the default close animation.
        if (m_transitionMgr && m_transitionMgr->isInitialized()) {
            const QRect rectGeo = m_animationDisabled ? QRect() : animationRect();
            auto *rawRect = m_transitionMgr->get_window_transition_rect(rawToken);
            if (rawRect) {
                // Each launched window gets its own persistent rect. The rect
                // stays alive (associated with that window) so it can drive
                // that window's close animation. We must NOT destroy a
                // previous rect here: doing so clears the close animation of
                // a still-open earlier window.
                auto *rect = new WindowTransitionRect(rawRect, this);
                // When the compositor reports that the rect's target window
                // has closed, the rect is useless: destroy it and drop it
                // from our list.
                connect(rect, &WindowTransitionRect::closed, this, [this, rect] {
                    const int idx = m_rects.indexOf(rect);
                    if (idx >= 0)
                        m_rects.removeAt(idx);
                    rect->destroy();
                });
                rect->set_geometry(rectGeo.x(), rectGeo.y(), rectGeo.width(), rectGeo.height());
                // Attach the launch-icon image as the animation source. The
                // compositor samples it immediately and uses it as the
                // starting visual of the open animation (and the ending
                // visual of the close animation).
                if (auto *display = getWlDisplay(this)) {
                    if (m_commitSourceBuffer)
                        rect->setSourceImage(display, m_image);
                    else
                        rect->clearSourceImage();
                }
                m_rects.append(rect);
            }
        }

        // 4. Commit the token to receive the token string
        token->commit();
        return token;
    }

    void launchReceiver()
    {
        auto *token = createLaunchToken();
        if (!token)
            return;
        connect(token, &ActivationToken::done, this, &SenderWindow::onReceiverToken);
    }

    void launchModal()
    {
        auto *token = createLaunchToken();
        if (!token)
            return;
        connect(token, &ActivationToken::done, this, &SenderWindow::onModalToken);
    }

    void onReceiverToken(const QString &tokenStr)
    {
        qInfo() << "Got activation token, launching receiver...";

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("XDG_ACTIVATION_TOKEN", tokenStr);

        // Do NOT parent the QProcess to this window: QProcess kills its
        // child on destruction, so closing the sender would tear down the
        // launched receiver along with it. Keep it parentless and let it
        // self-clean once the receiver exits.
        auto *proc = new QProcess;
        proc->setProcessEnvironment(env);
        connect(proc, &QProcess::finished, proc, &QProcess::deleteLater);
        proc->start(QCoreApplication::applicationFilePath(), { "--receiver" });
    }

    void onModalToken(const QString &tokenStr)
    {
        qInfo() << "Got activation token, showing modal...";

        if (!m_actMgr || !m_actMgr->isInitialized()) {
            qWarning() << "xdg_activation_v1 not available";
            return;
        }

        // A modal QDialog maps to an xdg_toplevel with set_parent + xdg_dialog
        // set_modal. WA_ShowWithoutActivating makes Qt skip its own
        // requestActivateOnShow, so we drive activation (and thus the window
        // transition) with the token we just created, mirroring the receiver.
        auto *dlg = new QDialog(this);
        dlg->setWindowTitle("Modal (Enter)");
        dlg->setModal(true);
        dlg->setAttribute(Qt::WA_ShowWithoutActivating, true);
        dlg->setAttribute(Qt::WA_DeleteOnClose, true);

        auto *layout = new QVBoxLayout(dlg);
        auto *label = new QLabel("Modal launched via xdg_activation + window transition rect", dlg);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);
        dlg->resize(400, 300);

        dlg->show();

        ::wl_surface *surface = getWlSurface(dlg);
        if (!surface) {
            qWarning() << "Cannot get wl_surface for modal";
            return;
        }
        m_actMgr->activate(tokenStr, surface);
    }

    XdgActivationManager *m_actMgr;
    WindowTransitionManager *m_transitionMgr;
    QList<WindowTransitionRect *> m_rects;
    QPixmap m_image;
    QWidget *m_imageWidget = nullptr;
    QLabel *m_sourceBufferIndicator = nullptr;
    bool m_commitSourceBuffer = true;
    bool m_animationDisabled = false;
};

// ---------------------------------------------------------------------------
// Receiver (Application B)
// ---------------------------------------------------------------------------

class ReceiverWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ReceiverWindow(XdgActivationManager *actMgr)
        : m_actMgr(actMgr)
        , m_image(createTestImage(800, 600))
    {
        setWindowTitle("Window Transition - Receiver");
        resize(800, 600);

        auto *central = new ImageWidget(m_image, this);
        setCentralWidget(central);
    }

    void activateWithToken()
    {
        QByteArray tokenEnv = qgetenv("XDG_ACTIVATION_TOKEN");
        if (tokenEnv.isEmpty()) {
            qWarning() << "No XDG_ACTIVATION_TOKEN set";
            return;
        }
        qputenv("XDG_ACTIVATION_TOKEN", "");

        if (!m_actMgr || !m_actMgr->isInitialized()) {
            qWarning() << "xdg_activation_v1 not available";
            return;
        }

        ::wl_surface *surface = getWlSurface(this);
        if (!surface) {
            qWarning() << "Cannot get wl_surface for receiver";
            return;
        }

        qInfo() << "Activating with token...";
        m_actMgr->activate(QString::fromUtf8(tokenEnv), surface);
    }

private:
    XdgActivationManager *m_actMgr;
    QPixmap m_image;
};

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    bool receiverMode = false;
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--receiver") == 0)
            receiverMode = true;
    }

    XdgActivationManager actMgr;
    WindowTransitionManager transitionMgr;

    if (receiverMode) {
        ReceiverWindow window(&actMgr);
        window.show();
        window.activateWithToken();
        return app.exec();
    } else {
        SenderWindow window(&actMgr, &transitionMgr);
        window.show();
        return app.exec();
    }
}

#include "main.moc"
