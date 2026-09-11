#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QTimer>

#include "screencapture.h"
#include "snapproxy.h"

class BackgroundImageProvider : public QQuickImageProvider
{
public:
    BackgroundImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        Q_UNUSED(id);
        Q_UNUSED(requestedSize);
        if (size)
            *size = m_image.size();
        return m_image;
    }

    void setImage(const QImage &image)
    {
        m_image = image;
    }

private:
    QImage m_image;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Treeland capture-snap demo client");
    parser.addHelpOption();
    const QCommandLineOption backgroundOption(
        QStringLiteral("background"),
        QStringLiteral("Capture the whole canvas with ext-image-copy-capture before showing the "
                       "selection UI and use it as the window background."));
    parser.addOption(backgroundOption);
    parser.process(app);

    SnapController controller;

    auto *backgroundProvider = new BackgroundImageProvider;
    QQmlApplicationEngine engine;
    engine.addImageProvider("captureBackground", backgroundProvider);
    engine.rootContext()->setContextProperty("snapController", &controller);

    {
        ScreenCapture sizeCapture;
        const QRect desktop = sizeCapture.desktopRect();
        if (desktop.isValid() && !desktop.isEmpty()) {
            controller.setCanvasX(desktop.x());
            controller.setCanvasY(desktop.y());
            controller.setCanvasWidth(desktop.width());
            controller.setCanvasHeight(desktop.height());
        }
    }

    if (parser.isSet(backgroundOption)) {
        ScreenCapture capture;
        const QImage canvas = capture.captureFullCanvas();
        if (!canvas.isNull()) {
            backgroundProvider->setImage(canvas);
            controller.setBackgroundEnabled(true);
        } else {
            qWarning() << "Background capture failed, falling back to a transparent background";
        }
    }

    engine.loadFromModule("TestSnapCapture", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return -1;

    window->show();

    QTimer::singleShot(0, [&]() {
        controller.setup(window);
    });

    return app.exec();
}
