#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QGuiApplication>
#include <QHash>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>

#include <utility>

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
        Q_UNUSED(requestedSize);
        const QImage image = m_images.value(id);
        if (size)
            *size = image.size();
        return image;
    }

    static QString idForGeometry(const QRect &geometry)
    {
        return QStringLiteral("%1_%2_%3_%4")
            .arg(geometry.x())
            .arg(geometry.y())
            .arg(geometry.width())
            .arg(geometry.height());
    }

    void addImage(const QRect &geometry, const QImage &image)
    {
        m_images.insert(idForGeometry(geometry), image);
    }

    bool hasImage(const QString &id) const { return m_images.contains(id); }

private:
    QHash<QString, QImage> m_images;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Treeland snap-target demo client");
    parser.addHelpOption();
    const QCommandLineOption backgroundOption(
        QStringLiteral("background"),
        QStringLiteral("Capture each output with ext-image-copy-capture before showing the "
                       "selection UI and use the per-output images as window backgrounds."));
    parser.addOption(backgroundOption);
    parser.process(app);

    // Capture backgrounds before any mask surface is mapped, so the masks do
    // not show up in their own screenshots.
    QList<ScreenCapture::CapturedOutput> capturedOutputs;
    if (parser.isSet(backgroundOption)) {
        ScreenCapture capture;
        capturedOutputs = capture.captureOutputs();
        if (capturedOutputs.isEmpty())
            qWarning() << "Background capture failed, falling back to a transparent background";
    }

    // One snap session per process, shared by every per-output mask window.
    SnapSession session;

    auto *backgroundProvider = new BackgroundImageProvider;
    for (const auto &output : std::as_const(capturedOutputs))
        backgroundProvider->addImage(output.logicalGeometry, output.image);

    QQmlApplicationEngine engine;
    engine.addImageProvider("captureBackground", backgroundProvider);

    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qt/qml/TestSnapTarget/Main.qml")));
    if (component.isError()) {
        qWarning() << component.errors();
        return -1;
    }

    const QList<QScreen *> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        qWarning() << "No outputs available";
        return -1;
    }

    struct MaskWindow
    {
        SnapController *controller = nullptr;
        QQuickWindow *window = nullptr;
        QScreen *screen = nullptr;
    };

    QList<MaskWindow> masks;
    for (QScreen *screen : screens) {
        auto *controller = new SnapController(&session, &session);
        auto *context = new QQmlContext(engine.rootContext());
        context->setContextProperty(QStringLiteral("snapController"), controller);

        QObject *object = component.create(context);
        auto *window = qobject_cast<QQuickWindow *>(object);
        if (!window) {
            qWarning() << "Failed to create mask window for output" << screen->name()
                       << component.errors();
            delete object;
            continue;
        }

        window->setScreen(screen);

        if (parser.isSet(backgroundOption)) {
            const QString id = BackgroundImageProvider::idForGeometry(screen->geometry());
            if (backgroundProvider->hasImage(id)) {
                controller->setBackgroundSource(QStringLiteral("image://captureBackground/") + id);
                controller->setBackgroundEnabled(true);
            }
        }

        masks.append({ controller, window, screen });
    }

    if (masks.isEmpty())
        return -1;

    QTimer::singleShot(0, [&session, masks]() {
        for (const MaskWindow &mask : masks)
            mask.controller->setup(mask.window, mask.screen);
        session.start();
    });

    return app.exec();
}
