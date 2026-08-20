// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-xdg-activation-v1.h"
#include "qwayland-treeland-window-animation-v1.h"

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

class WindowAnimationManager
    : public QWaylandClientExtensionTemplate<WindowAnimationManager>
    , public QtWayland::treeland_window_animation_manager_v1
{
    Q_OBJECT
public:
    explicit WindowAnimationManager()
        : QWaylandClientExtensionTemplate<WindowAnimationManager>(1)
    {
    }
    ~WindowAnimationManager() override
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

class WindowAnimationRect : public QObject, public QtWayland::treeland_window_animation_rect_v1
{
    Q_OBJECT
public:
    explicit WindowAnimationRect(::treeland_window_animation_rect_v1 *obj, QObject *parent = nullptr)
        : QObject(parent)
        , QtWayland::treeland_window_animation_rect_v1(obj)
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
    p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, "Window\nAnimation");
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
    SenderWindow(XdgActivationManager *actMgr, WindowAnimationManager *animMgr)
        : m_actMgr(actMgr)
        , m_animMgr(animMgr)
        , m_image(createTestImage(200, 200))
    {
        setWindowTitle("Window Animation - Sender (press Space)");

        auto *central = new QWidget(this);
        auto *layout = new QVBoxLayout(central);

        m_imageWidget = new ImageWidget(m_image, central);
        m_imageWidget->setFixedSize(200, 200);
        layout->addWidget(m_imageWidget, 0, Qt::AlignCenter);

        auto *label = new QLabel("Press SPACE to launch receiver\n"
                                 "The rect stays alive for close animation.", central);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label);

        setCentralWidget(central);
        resize(400, 300);
    }

    ~SenderWindow() override
    {
        // Destroy any remaining rect objects so the compositor falls back
        // to the default close animation for the receiver.
        if (m_rect) {
            m_rect->destroy();
            m_rect = nullptr;
        }
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

        // 3. Attach window animation rect (relative to this surface).
        //    The rect is persistent: it stays alive after commit and is used
        //    for both the open and close animations. We keep a reference so
        //    the compositor can read the latest geometry when B's window
        //    closes. Destroying the rect (or disconnecting) makes the
        //    compositor fall back to the default close animation.
        if (m_animMgr && m_animMgr->isInitialized()) {
            QPoint imagePos = m_imageWidget->mapTo(this, QPoint(0, 0));
            auto *rawRect = m_animMgr->get_window_animation_rect(rawToken);
            if (rawRect) {
                // Destroy any previous rect before creating a new one.
                if (m_rect) {
                    m_rect->destroy();
                    m_rect = nullptr;
                }
                m_rect = new WindowAnimationRect(rawRect, this);
                m_rect->set_geometry(imagePos.x(), imagePos.y(), 200, 200);
                m_rect->commit();
                // NOTE: The rect is NOT destroyed here. It stays alive so the
                // compositor can use it for the close animation when B's
                // window is closed. The rect will be destroyed when this
                // sender window is closed (see destructor).
            }
        }

        // 4. Commit the token to receive the token string
        token->commit();
    }

    XdgActivationManager *m_actMgr;
    WindowAnimationManager *m_animMgr;
    WindowAnimationRect *m_rect = nullptr;
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
        setWindowTitle("Window Animation - Receiver");
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
    WindowAnimationManager animMgr;

    if (receiverMode) {
        ReceiverWindow window(&actMgr);
        window.show();
        window.activateWithToken();
        return app.exec();
    } else {
        SenderWindow window(&actMgr, &animMgr);
        window.show();
        return app.exec();
    }
}

#include "main.moc"
