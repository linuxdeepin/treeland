// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-treeland-personalization-manager-v1.h"

#include <private/qwaylandwindow_p.h>

#include <qwindow.h>

#include <QApplication>
#include <QBoxLayout>
#include <QDebug>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QtWaylandClient/QWaylandClientExtension>

class PersonalizationManager
    : public QWaylandClientExtensionTemplate<PersonalizationManager>
    , public QtWayland::treeland_personalization_manager_v1
{
    Q_OBJECT
public:
    explicit PersonalizationManager()
        : QWaylandClientExtensionTemplate<PersonalizationManager>(1)
    {
    }
};

class PersonalizationWindow
    : public QWaylandClientExtensionTemplate<PersonalizationWindow>
    , public QtWayland::treeland_personalization_window_context_v1
{
    Q_OBJECT
public:
    explicit PersonalizationWindow(struct ::treeland_personalization_window_context_v1 *object)
        : QWaylandClientExtensionTemplate<PersonalizationWindow>(1)
        , QtWayland::treeland_personalization_window_context_v1(object)
    {
    }

    void destroyContext()
    {
        destroy();
    }
};

class BlurTestWindow : public QWidget
{
    Q_OBJECT
public:
    BlurTestWindow()
        : m_manager(new PersonalizationManager)
    {
        setWindowTitle("Window Blur Test Tool");
        setAttribute(Qt::WA_TranslucentBackground);
        resize(640, 560);

        QVBoxLayout *mainLayout = new QVBoxLayout;

        m_statusLabel = new QLabel("Status: Waiting for personalization manager...");
        m_statusLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #2196F3;");
        mainLayout->addWidget(m_statusLabel);

        m_contextLabel = new QLabel("Context: null");
        m_contextLabel->setStyleSheet("font-size: 12px;");
        mainLayout->addWidget(m_contextLabel);

        m_blendLabel = new QLabel("Blend Mode: none");
        m_blendLabel->setStyleSheet("font-size: 12px;");
        mainLayout->addWidget(m_blendLabel);

        mainLayout->addWidget(createSeparator());

        QLabel *manualLabel = new QLabel("Manual Controls");
        manualLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
        mainLayout->addWidget(manualLabel);

        QHBoxLayout *contextLayout = new QHBoxLayout;
        QPushButton *createCtxBtn = new QPushButton("Create Context");
        QPushButton *destroyCtxBtn = new QPushButton("Destroy Context");
        contextLayout->addWidget(createCtxBtn);
        contextLayout->addWidget(destroyCtxBtn);
        mainLayout->addLayout(contextLayout);

        connect(createCtxBtn, &QPushButton::clicked, this, &BlurTestWindow::createContext);
        connect(destroyCtxBtn, &QPushButton::clicked, this, &BlurTestWindow::destroyContext);

        QHBoxLayout *blendLayout = new QHBoxLayout;
        QPushButton *transparentBtn = new QPushButton("Set Transparent");
        QPushButton *wallpaperBtn = new QPushButton("Set Wallpaper");
        QPushButton *blurBtn = new QPushButton("Set Blur");
        blendLayout->addWidget(transparentBtn);
        blendLayout->addWidget(wallpaperBtn);
        blendLayout->addWidget(blurBtn);
        mainLayout->addLayout(blendLayout);

        connect(transparentBtn, &QPushButton::clicked, this, [this]() {
            setBlendMode(PersonalizationWindow::blend_mode_transparent);
        });
        connect(wallpaperBtn, &QPushButton::clicked, this, [this]() {
            setBlendMode(PersonalizationWindow::blend_mode_wallpaper);
        });
        connect(blurBtn, &QPushButton::clicked, this, [this]() {
            setBlendMode(PersonalizationWindow::blend_mode_blur);
        });

        mainLayout->addWidget(createSeparator());

        QLabel *scenarioLabel = new QLabel("Test Scenarios");
        scenarioLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
        mainLayout->addWidget(scenarioLabel);

        QPushButton *scenarioA = new QPushButton(
            "A: Create + Blur -> Destroy (blur should disappear)");
        QPushButton *scenarioB = new QPushButton(
            "B: Create + Blur (blur should show)");
        QPushButton *scenarioC = new QPushButton(
            "C: Blur -> Destroy -> Create without blur (no blur)");
        QPushButton *scenarioD = new QPushButton(
            "D: Blur -> Destroy -> Create + Transparent (switch correctly)");

        scenarioA->setStyleSheet("text-align: left; padding: 6px;");
        scenarioB->setStyleSheet("text-align: left; padding: 6px;");
        scenarioC->setStyleSheet("text-align: left; padding: 6px;");
        scenarioD->setStyleSheet("text-align: left; padding: 6px;");

        mainLayout->addWidget(scenarioA);
        mainLayout->addWidget(scenarioB);
        mainLayout->addWidget(scenarioC);
        mainLayout->addWidget(scenarioD);

        connect(scenarioA, &QPushButton::clicked, this, &BlurTestWindow::runScenarioA);
        connect(scenarioB, &QPushButton::clicked, this, &BlurTestWindow::runScenarioB);
        connect(scenarioC, &QPushButton::clicked, this, &BlurTestWindow::runScenarioC);
        connect(scenarioD, &QPushButton::clicked, this, &BlurTestWindow::runScenarioD);

        m_logLabel = new QLabel("Log: Ready");
        m_logLabel->setStyleSheet("font-size: 11px; color: #666; word-wrap: true;");
        m_logLabel->setWordWrap(true);
        mainLayout->addWidget(m_logLabel);

        mainLayout->addStretch();
        setLayout(mainLayout);

        connect(m_manager, &PersonalizationManager::activeChanged, this, [this]() {
            if (m_manager->isActive()) {
                m_statusLabel->setText("Status: Personalization manager active");
                m_statusLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: #4CAF50;");
            }
        });
    }

    ~BlurTestWindow()
    {
        if (m_windowContext) {
            m_windowContext->destroyContext();
            m_windowContext->deleteLater();
            m_windowContext = nullptr;
        }
        if (m_manager) {
            delete m_manager;
            m_manager = nullptr;
        }
    }

private:
    QFrame *createSeparator()
    {
        QFrame *line = new QFrame;
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        return line;
    }

    void log(const QString &msg)
    {
        qDebug() << msg;
        m_logLabel->setText("Log: " + msg);
    }

    void createContext()
    {
        if (m_windowContext) {
            log("Context already exists, destroy it first");
            return;
        }

        QWindow *window = this->windowHandle();
        if (!window || !window->handle()) {
            log("ERROR: Window handle not available");
            return;
        }

        auto *waylandWindow = static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
        struct wl_surface *surface = waylandWindow->wlSurface();
        if (!surface) {
            log("ERROR: wl_surface not available");
            return;
        }

        m_windowContext = new PersonalizationWindow(m_manager->get_window_context(surface));
        m_contextLabel->setText("Context: active");
        m_blendLabel->setText("Blend Mode: none (not set)");
        log("Context created");
    }

    void destroyContext()
    {
        if (!m_windowContext) {
            log("No context to destroy");
            return;
        }

        m_windowContext->destroyContext();
        m_windowContext->deleteLater();
        m_windowContext = nullptr;
        m_contextLabel->setText("Context: null");
        m_blendLabel->setText("Blend Mode: context destroyed");
        log("Context destroyed - blur should disappear if it was set");
    }

    void setBlendMode(int mode)
    {
        if (!m_windowContext) {
            log("ERROR: No context, create one first");
            return;
        }

        m_windowContext->set_blend_mode(mode);

        QString modeName;
        switch (mode) {
        case PersonalizationWindow::blend_mode_transparent:
            modeName = "transparent";
            break;
        case PersonalizationWindow::blend_mode_wallpaper:
            modeName = "wallpaper";
            break;
        case PersonalizationWindow::blend_mode_blur:
            modeName = "blur";
            break;
        default:
            modeName = "unknown";
            break;
        }

        m_blendLabel->setText("Blend Mode: " + modeName);
        log("Set blend mode: " + modeName);
    }

    void runScenarioA()
    {
        if (m_scenarioRunning) {
            log("Scenario already running, wait for completion");
            return;
        }
        m_scenarioRunning = true;
        log("Scenario A: Create + Blur -> Destroy");

        if (m_windowContext) {
            destroyContext();
        }

        createContext();

        QTimer::singleShot(500, this, [this]() {
            setBlendMode(PersonalizationWindow::blend_mode_blur);
            log("Scenario A: Blur set, destroying context in 2s...");

            QTimer::singleShot(2000, this, [this]() {
                destroyContext();
                log("Scenario A: Context destroyed. Check: blur should have disappeared.");
                m_scenarioRunning = false;
            });
        });
    }

    void runScenarioB()
    {
        if (m_scenarioRunning) {
            log("Scenario already running, wait for completion");
            return;
        }
        m_scenarioRunning = true;
        log("Scenario B: Create + Blur (normal)");

        if (m_windowContext) {
            destroyContext();
        }

        createContext();

        QTimer::singleShot(500, this, [this]() {
            setBlendMode(PersonalizationWindow::blend_mode_blur);
            log("Scenario B: Blur should be visible now.");
            m_scenarioRunning = false;
        });
    }

    void runScenarioC()
    {
        if (m_scenarioRunning) {
            log("Scenario already running, wait for completion");
            return;
        }
        m_scenarioRunning = true;
        log("Scenario C: Blur -> Destroy -> Create without blur");

        if (m_windowContext) {
            destroyContext();
        }

        createContext();

        QTimer::singleShot(500, this, [this]() {
            setBlendMode(PersonalizationWindow::blend_mode_blur);
            log("Scenario C: Blur set, destroying context in 2s...");

            QTimer::singleShot(2000, this, [this]() {
                destroyContext();
                log("Scenario C: Context destroyed. Creating new context without blur in 1s...");

                QTimer::singleShot(1000, this, [this]() {
                    createContext();
                    log("Scenario C: New context created without setting blur. Check: no blur should be visible.");
                    m_scenarioRunning = false;
                });
            });
        });
    }

    void runScenarioD()
    {
        if (m_scenarioRunning) {
            log("Scenario already running, wait for completion");
            return;
        }
        m_scenarioRunning = true;
        log("Scenario D: Blur -> Destroy -> Create + Transparent");

        if (m_windowContext) {
            destroyContext();
        }

        createContext();

        QTimer::singleShot(500, this, [this]() {
            setBlendMode(PersonalizationWindow::blend_mode_blur);
            log("Scenario D: Blur set, destroying context in 2s...");

            QTimer::singleShot(2000, this, [this]() {
                destroyContext();
                log("Scenario D: Context destroyed. Creating new context with transparent in 1s...");

                QTimer::singleShot(1000, this, [this]() {
                    createContext();

                    QTimer::singleShot(500, this, [this]() {
                        setBlendMode(PersonalizationWindow::blend_mode_transparent);
                        log("Scenario D: New context with transparent mode. Check: blur should not be visible, window should be transparent.");
                        m_scenarioRunning = false;
                    });
                });
            });
        });
    }

    PersonalizationManager *m_manager = nullptr;
    PersonalizationWindow *m_windowContext = nullptr;
    bool m_scenarioRunning = false;

    QLabel *m_statusLabel = nullptr;
    QLabel *m_contextLabel = nullptr;
    QLabel *m_blendLabel = nullptr;
    QLabel *m_logLabel = nullptr;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    BlurTestWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
