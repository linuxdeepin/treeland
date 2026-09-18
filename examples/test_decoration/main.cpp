// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qwayland-treeland-decoration-unstable-v1.h"

#include <private/qwaylandwindow_p.h>

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QtWaylandClient/QWaylandClientExtension>

#include <limits>

// ---------------------------------------------------------------------------
// treeland_decoration_manager_v1 (client-side)
// ---------------------------------------------------------------------------

class DecorationManager
    : public QWaylandClientExtensionTemplate<DecorationManager>
    , public QtWayland::treeland_decoration_manager_v1
{
    Q_OBJECT
public:
    explicit DecorationManager();
};

DecorationManager::DecorationManager()
    : QWaylandClientExtensionTemplate<DecorationManager>(1)
{
}

// ---------------------------------------------------------------------------
// treeland_decoration_context_v1 (client-side)
// ---------------------------------------------------------------------------

class DecorationContext
    : public QWaylandClientExtensionTemplate<DecorationContext>
    , public QtWayland::treeland_decoration_context_v1
{
    Q_OBJECT
public:
    explicit DecorationContext(struct ::treeland_decoration_context_v1 *object);
};

DecorationContext::DecorationContext(struct ::treeland_decoration_context_v1 *object)
    : QWaylandClientExtensionTemplate<DecorationContext>(1)
    , QtWayland::treeland_decoration_context_v1(object)
{
}

// ---------------------------------------------------------------------------
// Input helpers
// ---------------------------------------------------------------------------

static bool readUInt32(QLineEdit *edit, uint32_t *out, const char *name)
{
    bool ok = false;
    const qulonglong value = edit->text().toULongLong(&ok);
    if (!ok || value > std::numeric_limits<uint32_t>::max()) {
        qWarning() << "invalid" << name << ":" << edit->text();
        return false;
    }
    *out = static_cast<uint32_t>(value);
    return true;
}

static bool readInt32(QLineEdit *edit, int32_t *out, const char *name)
{
    bool ok = false;
    const qlonglong value = edit->text().toLongLong(&ok);
    if (!ok || value < std::numeric_limits<int32_t>::min()
        || value > std::numeric_limits<int32_t>::max()) {
        qWarning() << "invalid" << name << ":" << edit->text();
        return false;
    }
    *out = static_cast<int32_t>(value);
    return true;
}

/// Adds a "label + QLineEdit" pair to @p grid at the given logical column.
static QLineEdit *addField(QGridLayout *grid, int row, int col, const QString &label,
                           const QString &placeholder)
{
    grid->addWidget(new QLabel(label), row, col * 2);
    auto *edit = new QLineEdit;
    edit->setPlaceholderText(placeholder);
    grid->addWidget(edit, row, col * 2 + 1);
    return edit;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    DecorationManager manager;

    QObject::connect(&manager, &DecorationManager::activeChanged, &manager, [&manager] {
        if (!manager.isActive())
            return;

        static QPointer<QWidget> window;
        if (window)
            return;

        window = new QWidget;
        window->setWindowTitle("Decoration Client (treeland-decoration-unstable-v1)");
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->setMinimumWidth(720);

        auto *vbox = new QVBoxLayout(window);

        // ---- Corner radius ----
        vbox->addWidget(new QLabel("Corner radius"));
        auto *radiusGrid = new QGridLayout;
        QLineEdit *radiusEdit = addField(radiusGrid, 0, 0, "radius", "0 = square");
        radiusEdit->setText("12");
        auto *radiusButton = new QPushButton("Set radius");
        vbox->addLayout(radiusGrid);
        vbox->addWidget(radiusButton);

        // ---- Shadow (radius, offset_x, offset_y, r, g, b, a) ----
        vbox->addWidget(new QLabel("Shadow"));
        auto *shadowGrid = new QGridLayout;
        QLineEdit *shadowRadius = addField(shadowGrid, 0, 0, "radius", "0 = off");
        shadowRadius->setText("40");
        QLineEdit *shadowOx = addField(shadowGrid, 0, 1, "offset_x", "pixels");
        shadowOx->setText("0");
        QLineEdit *shadowOy = addField(shadowGrid, 0, 2, "offset_y", "pixels");
        shadowOy->setText("10");
        QLineEdit *shadowR = addField(shadowGrid, 0, 3, "r", "0-255");
        shadowR->setText("0");
        QLineEdit *shadowG = addField(shadowGrid, 1, 0, "g", "0-255");
        shadowG->setText("0");
        QLineEdit *shadowB = addField(shadowGrid, 1, 1, "b", "0-255");
        shadowB->setText("0");
        QLineEdit *shadowA = addField(shadowGrid, 1, 2, "a", "0-255");
        shadowA->setText("102");
        auto *shadowButton = new QPushButton("Set shadow");
        vbox->addLayout(shadowGrid);
        vbox->addWidget(shadowButton);

        // ---- Border (width, r, g, b, a) ----
        vbox->addWidget(new QLabel("Border"));
        auto *borderGrid = new QGridLayout;
        QLineEdit *borderWidth = addField(borderGrid, 0, 0, "width", "0 = off");
        borderWidth->setText("1");
        QLineEdit *borderR = addField(borderGrid, 0, 1, "r", "0-255");
        borderR->setText("0");
        QLineEdit *borderG = addField(borderGrid, 0, 2, "g", "0-255");
        borderG->setText("0");
        QLineEdit *borderB = addField(borderGrid, 0, 3, "b", "0-255");
        borderB->setText("0");
        QLineEdit *borderA = addField(borderGrid, 1, 0, "a", "0-255");
        borderA->setText("26");
        auto *borderButton = new QPushButton("Set border");
        vbox->addLayout(borderGrid);
        vbox->addWidget(borderButton);

        // ---- Titlebar mode ----
        auto *modeRow = new QHBoxLayout;
        auto *modeCombo = new QComboBox;
        modeCombo->addItem("show", 0);
        modeCombo->addItem("hide", 1);
        auto *modeButton = new QPushButton("Set titlebar");
        modeRow->addWidget(new QLabel("Titlebar mode"));
        modeRow->addWidget(modeCombo, 1);
        modeRow->addWidget(modeButton);
        vbox->addLayout(modeRow);

        window->show();

        // The controls live in the same window as the decorated surface.
        QWindow *qWindow = window->windowHandle();
        if (!qWindow || !qWindow->handle()) {
            qWarning() << "no Wayland window handle";
            return;
        }
        auto *waylandWindow = static_cast<QtWaylandClient::QWaylandWindow *>(qWindow->handle());
        struct wl_surface *surface = waylandWindow->wlSurface();
        if (!surface) {
            qWarning() << "no wl_surface";
            return;
        }
        auto *context = new DecorationContext(manager.get_decoration_context(surface));

        QObject::connect(radiusButton, &QPushButton::clicked, radiusButton, [context, radiusEdit] {
            uint32_t radius = 0;
            if (!readUInt32(radiusEdit, &radius, "radius"))
                return;
            context->set_corner_radius(radius);
            qDebug() << "set_corner_radius(" << radius << ")";
        });

        QObject::connect(shadowButton, &QPushButton::clicked, shadowButton,
                         [context, shadowRadius, shadowOx, shadowOy, shadowR, shadowG, shadowB,
                          shadowA] {
                             uint32_t radius = 0;
                             int32_t ox = 0;
                             int32_t oy = 0;
                             uint32_t r = 0;
                             uint32_t g = 0;
                             uint32_t b = 0;
                             uint32_t a = 0;
                             if (!readUInt32(shadowRadius, &radius, "shadow radius")
                                 || !readInt32(shadowOx, &ox, "shadow offset_x")
                                 || !readInt32(shadowOy, &oy, "shadow offset_y")
                                 || !readUInt32(shadowR, &r, "shadow r")
                                 || !readUInt32(shadowG, &g, "shadow g")
                                 || !readUInt32(shadowB, &b, "shadow b")
                                 || !readUInt32(shadowA, &a, "shadow a"))
                                 return;
                             context->set_shadow(radius, ox, oy, r, g, b, a);
                             qDebug() << "set_shadow(" << radius << ox << oy << r << g << b << a
                                      << ")";
                         });

        QObject::connect(borderButton, &QPushButton::clicked, borderButton,
                         [context, borderWidth, borderR, borderG, borderB, borderA] {
                             uint32_t width = 0;
                             uint32_t r = 0;
                             uint32_t g = 0;
                             uint32_t b = 0;
                             uint32_t a = 0;
                             if (!readUInt32(borderWidth, &width, "border width")
                                 || !readUInt32(borderR, &r, "border r")
                                 || !readUInt32(borderG, &g, "border g")
                                 || !readUInt32(borderB, &b, "border b")
                                 || !readUInt32(borderA, &a, "border a"))
                                 return;
                             context->set_border(width, r, g, b, a);
                             qDebug() << "set_border(" << width << r << g << b << a << ")";
                         });

        QObject::connect(modeButton, &QPushButton::clicked, modeButton, [context, modeCombo] {
            const uint32_t mode = uint32_t(modeCombo->currentData().toUInt());
            context->set_titlebar_mode(mode);
            qDebug() << "set_titlebar_mode(" << modeCombo->currentText() << ")";
        });
    });

    return app.exec();
}

#include "main.moc"
