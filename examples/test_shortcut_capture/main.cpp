// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

// Test application for treeland_shortcut_capture_v1 (treeland-shortcut-manager-v2, version 2).
// A window with a single button: click to request one-shot shortcut capture.
// The captured key sequence or failure reason is shown below the button.

#include "qwayland-treeland-shortcut-manager-v2.h"

#include <private/qwaylandwindow_p.h>

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWaylandClientExtension>
#include <QWidget>

// Wraps the treeland_shortcut_capture_v1 Wayland object and emits Qt signals.
class ShortcutCapture
    : public QObject
    , public QtWayland::treeland_shortcut_capture_v1
{
    Q_OBJECT
public:
    explicit ShortcutCapture(struct ::treeland_shortcut_capture_v1 *capture,
                             QObject *parent = nullptr)
        : QObject(parent)
        , QtWayland::treeland_shortcut_capture_v1(capture)
    {
    }

    ~ShortcutCapture() override
    {
        destroy();
    }

Q_SIGNALS:
    void captured(const QString &key);
    void failed(uint32_t reason);

protected:
    void treeland_shortcut_capture_v1_captured(const QString &key) override
    {
        emit captured(key);
        deleteLater();
    }

    void treeland_shortcut_capture_v1_failed(uint32_t reason) override
    {
        emit failed(reason);
        deleteLater();
    }
};

class ShortcutManagerV2
    : public QWaylandClientExtensionTemplate<ShortcutManagerV2>
    , public QtWayland::treeland_shortcut_manager_v2
{
    Q_OBJECT
public:
    explicit ShortcutManagerV2()
        : QWaylandClientExtensionTemplate<ShortcutManagerV2>(2)
    {
    }

    // Request one-shot shortcut capture for the given window surface.
    // Returns a ShortcutCapture object whose signals deliver the result.
    ShortcutCapture *captureNextShortcut(QWindow *window, QObject *parent = nullptr)
    {
        if (!window || !window->handle())
            return nullptr;
        auto *waylandWindow =
            static_cast<QtWaylandClient::QWaylandWindow *>(window->handle());
        auto *wlSurface = waylandWindow->wlSurface();
        if (!wlSurface)
            return nullptr;

        auto *raw = capture_next_shortcut(wlSurface, nullptr);
        if (!raw)
            return nullptr;
        return new ShortcutCapture(raw, parent);
    }
};

static QString cancelReasonText(uint32_t reason)
{
    using R = QtWayland::treeland_shortcut_capture_v1;
    switch (static_cast<R::failed_reason>(reason)) {
    case R::failed_reason_busy:
        return QStringLiteral("Another capture is already in progress (busy)");
    case R::failed_reason_aborted:
        return QStringLiteral("The compositor aborted the capture (aborted)");
    case R::failed_reason_not_active:
        return QStringLiteral("The window is not active (not_active)");
    case R::failed_reason_interrupted:
        return QStringLiteral("Capture was interrupted by non-shortcut input (interrupted)");
    default:
        return QStringLiteral("Unknown reason (%1)").arg(reason);
    }
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);

    ShortcutManagerV2 manager;

    // Main window
    QWidget window;
    window.setWindowTitle(QStringLiteral("Shortcut Capture Test"));
    window.resize(360, 160);

    QVBoxLayout *layout = new QVBoxLayout(&window);

    QPushButton *btn = new QPushButton(QStringLiteral("Request Shortcut Capture"), &window);
    QLabel *statusLabel = new QLabel(QStringLiteral("Click the button, then press any shortcut..."), &window);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setWordWrap(true);

    layout->addWidget(btn);
    layout->addWidget(statusLabel);

    // Disable button until the protocol is available
    btn->setEnabled(false);
    QObject::connect(&manager, &ShortcutManagerV2::activeChanged, btn, [&] {
        btn->setEnabled(manager.isActive());
    });

    QObject::connect(btn, &QPushButton::clicked, &window, [&] {
        if (!manager.isActive()) {
            statusLabel->setText(QStringLiteral("Protocol is not ready yet, please try again later."));
            return;
        }

        btn->setEnabled(false);
        statusLabel->setText(QStringLiteral("Waiting for shortcut input..."));

        ShortcutCapture *capture = manager.captureNextShortcut(window.windowHandle(), &window);
        if (!capture) {
            statusLabel->setText(QStringLiteral("Failed to create capture object (Wayland surface not ready?)"));
            btn->setEnabled(true);
            return;
        }

        QObject::connect(capture, &ShortcutCapture::captured, &window,
                         [statusLabel, btn](const QString &key) {
                             statusLabel->setText(QStringLiteral("Captured shortcut: <b>%1</b>").arg(key));
                             btn->setEnabled(true);
                         });

        QObject::connect(capture, &ShortcutCapture::failed, &window,
                         [statusLabel, btn](uint32_t reason) {
                             statusLabel->setText(QStringLiteral("Capture failed: %1").arg(cancelReasonText(reason)));
                             btn->setEnabled(true);
                         });
    });

    window.show();
    return app.exec();
}

#include "main.moc"
