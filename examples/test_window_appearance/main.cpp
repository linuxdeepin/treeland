// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "appearance_manager.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDebug>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include <wayland-client.h>

class TestWindow : public QWidget
{
    Q_OBJECT
public:
    TestWindow()
        : m_observer(new AppearanceObserver)
        , m_manager(new AppearanceManager)
    {
        setWindowTitle("Appearance Client (0.6.0)");

        m_mainLayout = new QVBoxLayout;

        // ---- Color scheme ----
        auto *scheme = addSettingRow("color scheme", {}, {});
        scheme->edit->hide();
        auto *lightBtn = new QPushButton("light");
        auto *darkBtn = new QPushButton("dark");
        for (auto *button : { lightBtn, darkBtn })
            scheme->layout->addWidget(button);
        connect(lightBtn, &QPushButton::clicked, this, &TestWindow::setLightScheme);
        connect(darkBtn, &QPushButton::clicked, this, &TestWindow::setDarkScheme);
        connect(m_observer, &AppearanceObserver::colorSchemeChanged, this, [this, scheme](uint32_t value) {
            updateRow(*scheme, value == QtWayland::treeland_appearance_v1::color_scheme_dark
                        ? "dark" : "light");
        });

        // ---- Titlebar height ----
        auto *titlebar = addSettingRow(
            "titlebar height", "titlebar height in pixels", [this](const QString &text) {
                m_manager->set_titlebar_height(text.toUInt());
            });
        connect(m_observer, &AppearanceObserver::titlebarHeightChanged, this, [this, titlebar](uint32_t height) {
            updateRow(*titlebar, QString::number(height));
        });

        // ---- Corner radius ----
        auto *radius = addSettingRow("corner radius", "corner radius in pixels", [this](const QString &text) {
            m_manager->set_corner_radius(text.toUInt());
        });
        connect(m_observer, &AppearanceObserver::cornerRadiusChanged, this, [this, radius](uint32_t value) {
            updateRow(*radius, QString::number(value));
        });

        // ---- Cursor theme ----
        auto *cursorTheme = addSettingRow("cursor theme", "cursor theme name", [this](const QString &text) {
            m_manager->set_cursor_theme(text);
        });
        connect(m_observer, &AppearanceObserver::cursorThemeChanged, this, [this, cursorTheme](const QString &name) {
            updateRow(*cursorTheme, name);
        });

        // ---- Cursor size ----
        auto *cursorSize = addSettingRow("cursor size", "cursor size in pixels", [this](const QString &text) {
            const uint32_t size = text.toUInt();
            m_manager->set_cursor_size(size, size);
        });
        connect(m_observer, &AppearanceObserver::cursorSizeChanged, this, [this, cursorSize](uint32_t width, uint32_t height) {
            updateRow(*cursorSize, QString("%1x%2").arg(width).arg(height));
        });

        // ---- Font ----
        auto *font = addSettingRow("font", "font family name", [this](const QString &text) {
            m_manager->set_font(text);
        });
        connect(m_observer, &AppearanceObserver::fontChanged, this, [this, font](const QString &name) {
            updateRow(*font, name);
        });

        // ---- Monospace font ----
        auto *monoFont = addSettingRow("monospace font", "monospace font family name", [this](const QString &text) {
            m_manager->set_monospace_font(text);
        });
        connect(m_observer, &AppearanceObserver::monospaceFontChanged, this, [this, monoFont](const QString &name) {
            updateRow(*monoFont, name);
        });

        // ---- Font size ----
        auto *fontSize = addSettingRow("font size", "font size in points", [this](const QString &text) {
            m_manager->set_font_size(text.toUInt());
        });
        connect(m_observer, &AppearanceObserver::fontSizeChanged, this, [this, fontSize](uint32_t size) {
            updateRow(*fontSize, QString::number(size));
        });

        // ---- Icon theme ----
        auto *iconTheme = addSettingRow("icon theme", "icon theme name", [this](const QString &text) {
            m_manager->set_icon_theme(text);
        });
        connect(m_observer, &AppearanceObserver::iconThemeChanged, this, [this, iconTheme](const QString &name) {
            updateRow(*iconTheme, name);
        });

        // ---- Accent color ----
        auto *accentColor = addSettingRow(
            "accent color", "r,g,b (each 0-255), e.g. 44,119,222",
            [this](const QString &text) {
                const auto parts = text.split(',');
                if (parts.size() != 3) {
                    qWarning() << "accent color must be 3 comma-separated values";
                    return;
                }
                m_manager->set_accent_color(parts[0].toUInt(), parts[1].toUInt(),
                                            parts[2].toUInt());
            });
        connect(m_observer, &AppearanceObserver::accentColorChanged, this, [this, accentColor](uint32_t r, uint32_t g, uint32_t b) {
            updateRow(*accentColor, QString("%1,%2,%3").arg(r).arg(g).arg(b));
        });

        // ---- Window opacity ----
        auto *opacity = addSettingRow(
            "window opacity", "opacity [0.0, 1.0], e.g. 0.8", [this](const QString &text) {
                bool ok = false;
                const double value = text.toDouble(&ok);
                if (!ok || value < 0.0 || value > 1.0) {
                    qWarning() << "window opacity must be in [0.0, 1.0]:" << text;
                    return;
                }
                m_manager->set_window_opacity(wl_fixed_from_double(value));
            });
        connect(m_observer, &AppearanceObserver::windowOpacityChanged, this, [this, opacity](wl_fixed_t value) {
            updateRow(*opacity, QString::number(wl_fixed_to_double(value), 'f', 2));
        });

        m_mainLayout->addStretch();

        qDebug() << "appearance observer active:" << m_observer->isActive()
                 << "manager active:" << m_manager->isActive();

        setLayout(m_mainLayout);
        setMinimumSize(480, 560);
    }

private:
    struct SettingRow
    {
        QLabel *label = nullptr;
        QLineEdit *edit = nullptr;
        QHBoxLayout *layout = nullptr;
        QString title;
    };

    // Creates a row of "title: <current value>" plus an editable field and an
    // apply button wired to `apply`. The returned row's label/edit is kept in
    // sync by the corresponding observer-signal connection at the call site.
    SettingRow *addSettingRow(const QString &title, const QString &placeholder,
                              const std::function<void(const QString &)> &apply)
    {
        auto *label = new QLabel(QString("%1: -").arg(title));
        auto *edit = new QLineEdit;
        edit->setPlaceholderText(placeholder);
        auto *button = new QPushButton(QString("set %1").arg(title));

        auto *rowLayout = new QHBoxLayout;
        rowLayout->addWidget(label, 1);
        rowLayout->addWidget(edit, 2);

        // Rows are owned by unique_ptr so that their addresses stay stable
        // as more rows are added; observer connections capture the pointer.
        auto row = std::make_unique<SettingRow>(
            SettingRow { label, edit, rowLayout, title });
        auto *rowPtr = row.get();
        m_rows.push_back(std::move(row));
        rowLayout->addWidget(button);

        m_mainLayout->addLayout(rowLayout);

        if (apply) {
            connect(button, &QPushButton::clicked, this, [this, edit, apply] { apply(edit->text()); });
        } else {
            button->hide();
        }

        return rowPtr;
    }

    void updateRow(SettingRow &row, const QString &value)
    {
        row.label->setText(QString("%1: %2").arg(row.title, value));
        row.edit->blockSignals(true);
        row.edit->setText(value);
        row.edit->blockSignals(false);
    }

    void setLightScheme()
    {
        m_manager->set_color_scheme(QtWayland::treeland_appearance_manager_v1::color_scheme_light);
    }

    void setDarkScheme()
    {
        m_manager->set_color_scheme(QtWayland::treeland_appearance_manager_v1::color_scheme_dark);
    }

    std::vector<std::unique_ptr<SettingRow>> m_rows;
    QVBoxLayout *m_mainLayout = nullptr;
    AppearanceObserver *m_observer = nullptr;
    AppearanceManager *m_manager = nullptr;
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    TestWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
