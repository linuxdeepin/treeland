// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QApplication>
#include <QObject>
#include <QScreen>
#include <QWaylandClientExtension>
#include <QCommandLineParser>

#include "qwayland-treeland-output-manager-v1.h"


static wl_output *wlOutputForName(const QString &name)
{
    auto pni = QGuiApplication::platformNativeInterface();
    if (!pni) {
        qWarning() << "Not running on a platform with a native interface (Wayland?).";
        return nullptr;
    }

    for (QScreen *screen : QGuiApplication::screens()) {
        const QString sname = screen->name(); // from xdg-output name on Wayland
        if (QString::compare(sname, name, Qt::CaseInsensitive) == 0) {
            auto *res = screen->nativeInterface<QNativeInterface::QWaylandScreen>();
            return res->output();
        }
    }
    return nullptr;
}

class OutputManagerV1
    : public QWaylandClientExtensionTemplate<OutputManagerV1>
    , public QtWayland::treeland_output_manager_v1
{
    Q_OBJECT
public:
    explicit OutputManagerV1()
        : QWaylandClientExtensionTemplate<OutputManagerV1>(2)
    {

    }

    void treeland_output_manager_v1_primary_output(const QString &output_name)
    {
        qInfo() << "received primary_output: " << output_name;
    }
};

class ColorControlV1
    : public QWaylandClientExtensionTemplate<ColorControlV1>
    , public QtWayland::treeland_output_color_control_v1
{
    Q_OBJECT
public:
    explicit ColorControlV1(::treeland_output_color_control_v1 *nativeControl)
        : QWaylandClientExtensionTemplate<ColorControlV1>(1)
        , QtWayland::treeland_output_color_control_v1(nativeControl)
    {

    }
protected:
    void treeland_output_color_control_v1_color_temperature(uint32_t color_temperature) override
    {
        qInfo() << "received color_temperature: " << color_temperature;
    }
    void treeland_output_color_control_v1_brightness(wl_fixed_t brightness) override
    {
        qInfo() << "received brightness: " << wl_fixed_to_double(brightness);
    }
    void treeland_output_color_control_v1_result(uint32_t success) override
    {
        qInfo() << "received commit result: " << (success ? "success" : "failure");
    }
};

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("Test Color Control V1");
    parser.addHelpOption();
    QCommandLineOption colorTempOption(QStringList() << "c"
                                                            << "color-temperature",
                                       "Color temperature to set (in Kelvin).",
                                       "color-temperature");
    QCommandLineOption brightnessOption(QStringList() << "b"
                                                             << "brightness",
                                        "Brightness to set (0.0 - 100.0).",
                                        "brightness");
    parser.addOption(colorTempOption);
    parser.addOption(brightnessOption);
    parser.addPositionalArgument("output", "Name of the output to control.");
    parser.process(QApplication::arguments());
    auto colorTemperature = parser.value(colorTempOption).toUInt();
    auto brightness = parser.value(brightnessOption).toDouble();
    auto outputName = parser.positionalArguments().value(0);

    auto colorTemperatureSet = parser.isSet(colorTempOption);;
    auto brightnessSet = parser.isSet(brightnessOption);

    OutputManagerV1 manager;
    manager.setParent(&app);
    QObject::connect(&manager, &OutputManagerV1::activeChanged, &manager, [&] {
        if (manager.isActive()) {
            auto output = wlOutputForName(outputName);
            treeland_output_color_control_v1 *native_control = nullptr;
            ColorControlV1 *colorControl = nullptr;
            if (output) {
                native_control = manager.get_color_control(output);
                if (native_control) {
                    colorControl = new ColorControlV1(native_control);
                    colorControl->setParent(&app);
                    if (colorTemperatureSet) {
                        qInfo() << "setting color temperature to " << colorTemperature;
                        colorControl->set_color_temperature(colorTemperature);
                    }
                    if (brightnessSet) {
                        qInfo() << "setting brightness to " << brightness;
                        colorControl->set_brightness(wl_fixed_from_double(brightness));
                    }
                    if (colorTemperatureSet || brightnessSet) {
                        qInfo() << "committing changes";
                        colorControl->commit();
                    }
                }
            }
        }
    });
    app.exec();
    return 0;
}

#include "main.moc"
