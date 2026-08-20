// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-xdg-activation-v1.h"
#include "qwayland-treeland-launch-animation-v1.h"

#include <private/qwaylandwindow_p.h>

#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QPainter>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScreen>
#include <QVBoxLayout>
#include <QWidget>
#include <QtWaylandClient/QWaylandClientExtension>

#include <cstdlib>

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

class LaunchAnimationManager
    : public QWaylandClientExtensionTemplate<LaunchAnimationManager>
    , public QtWayland::treeland_launch_animation_manager_v1
{
    Q_OBJECT
public:
    explicit LaunchAnimationManager()
        : QWaylandClientExtensionTemplate<LaunchAnimationManager>(1)
    {
    }
    ~LaunchAnimationManager() override
    {
        if (isInitialized())
            destroy();
    }
};

// ---------------------------------------------------------------------------
// Per-object wrappers
// ---------------------------------------------------------------------------

class ActivationToken : public QObject, public QtWayland::xdg_activation_token_v1
{
    Q_OBJECT
public:
    explicit ActivationToken(::xdg_activation_token_v1 *obj, QObject *parent = nullptr)
        : QObject(parent)
        , QtWayland::xdg_activation_token_v1(obj)
    {
    }

    QString tokenString() const { return m_token; }

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

class LaunchRect : public QObject, public QtWayland::treeland_launch_rect_v1
{
    Q_OBJECT
public:
    explicit LaunchRect(::treeland_launch_rect_v1 *obj, QObject *parent = nullptr)
        : QObject(parent)
        , QtWayland::treeland_launch_rect_v1(obj)
    {
    }
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
    p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, "Launch\nAnimation");
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
        p.drawPixmap(0, 0, m_pixmap.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    }

private:
    QPixmap m_pixmap;
};

// ---------------------------------------------------------------------------
// Helper: get wl_surface from a widget
// ---------------------------------------------------------------------------

static ::wl_surface *getWlSurface(QWidget *widget)
{
    // Do not call show()/processEvents() here: that would map the surface
    // (completing the configure round-trip) before the caller can request
    // activation, breaking the "activate before first map" timing. Callers
    // ensure the widget is already shown so the platform window and its
    // wl_surface exist, while the surface itself stays unmapped until the
    // event loop runs.
    QWindow *handle = widget->windowHandle();
    if (!handle || !handle->handle())
        return nullptr;
    auto *waylandWindow = static_cast<QtWaylandClient::QWaylandWindow *>(handle->handle());
    return waylandWindow ? waylandWindow->surface() : nullptr;
}

// ---------------------------------------------------------------------------
// Sender (Application A)
// ---------------------------------------------------------------------------

class SenderWindow : public QMainWindow
{
    Q_OBJECT
public:
    SenderWindow(XdgActivationManager *actMgr, LaunchAnimationManager *launchMgr)
        : m_actMgr(actMgr)
        , m_launchMgr(launchMgr)
        , m_image(createTestImage(200, 200))
    {
        setWindowTitle("Launch Animation - Sender (press Space)");
        resize(400, 300);

        auto *central = new QWidget(this);
        auto *layout = new QVBoxLayout(central);

        m_imageWidget = new ImageWidget(m_image, central);
        m_imageWidget->setFixedSize(200, 200);
        layout->addWidget(m_imageWidget, 0, Qt::AlignCenter);

        auto *label = new QLabel("Press SPACE to launch receiver", central);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);

        setCentralWidget(central);
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Space)
            launchReceiver();
        QMainWindow::keyPressEvent(event);
    }

private:
    void launchReceiver()
    {
        if (!m_actMgr || !m_actMgr->isInitialized()) {
            qWarning() << "xdg_activation_v1 not available";
            return;
        }

        ::wl_surface *surface = getWlSurface(this);
        if (!surface) {
            qWarning() << "Cannot get wl_surface";
            return;
        }

        // 1. Create activation token
        auto *rawToken = m_actMgr->get_activation_token();
        if (!rawToken) {
            qWarning() << "Failed to create activation token";
            return;
        }
        auto *token = new ActivationToken(rawToken, this);

        connect(token, &ActivationToken::done, this, [this](const QString &tokenStr) {
            qInfo() << "Got activation token, launching receiver...";

            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert("XDG_ACTIVATION_TOKEN", tokenStr);

            QProcess *proc = new QProcess(this);
            proc->setProcessEnvironment(env);
            proc->start(QCoreApplication::applicationFilePath(), {"--receiver"});
        });

        // 2. Set the originating surface on the token
        token->set_surface(surface);

        // 3. Attach launch rect (relative to this surface)
        if (m_launchMgr && m_launchMgr->isInitialized()) {
            QPoint imagePos = m_imageWidget->mapTo(this, QPoint(0, 0));
            auto *rawRect = m_launchMgr->get_launch_rect(rawToken);
            if (rawRect) {
                auto *rect = new LaunchRect(rawRect, this);
                rect->set_geometry(imagePos.x(), imagePos.y(), 200, 200);
                rect->commit();
                // Keep the rect object alive until the token is committed: the
                // compositor consumes the committed rectangle in token.commit,
                // so destroying the rect first would race the consume on a
                // single ordered Wayland connection. The rect is inert after
                // commit, so it only needs to outlive the token commit below.
                m_pendingRect = rect;
            }
        }

        // 4. Commit the token to receive the token string
        token->commit();
        // Now that the compositor has consumed the rectangle, destroy the rect
        // object (it is inert after commit).
        if (m_pendingRect) {
            m_pendingRect->destroy();
            m_pendingRect = nullptr;
        }
    }

    XdgActivationManager *m_actMgr;
    LaunchAnimationManager *m_launchMgr;
    LaunchRect *m_pendingRect = nullptr;
    QPixmap m_image;
    QWidget *m_imageWidget = nullptr;
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
        setWindowTitle("Launch Animation - Receiver");
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
    LaunchAnimationManager launchMgr;

    if (receiverMode) {
        ReceiverWindow window(&actMgr);
        // show() creates the xdg_toplevel and wl_surface but, per the Wayland
        // configure cycle, leaves the surface unmapped until the event loop
        // processes the configure round-trip. Activate now — before app.exec()
        // runs the event loop — so the compositor stores the launch rect while
        // the surface is still unmapped, then plays it on first map.
        window.show();
        window.activateWithToken();
        return app.exec();
    } else {
        SenderWindow window(&actMgr, &launchMgr);
        window.show();
        return app.exec();
    }
}

#include "main.moc"
