// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "abstractsettings.h"

/*## Common `Xresources` (X11) Settings

| Category                 | Property        | Example Value                                         | Description                                                                     |
| ------------------------ | --------------- | ----------------------------------------------------- | ------------------------------------------------------------------------------- |
| **Xft (Font Rendering)** | `Xft.dpi`       | `96` / `144` / `192`                                  | DPI setting used by font rendering. Often overridden by `xsettingsd` or `xrdb`. |
|                          | `Xft.antialias` | `1` or `0`                                            | Enables (1) or disables (0) font anti-aliasing.                                 |
|                          | `Xft.hinting`   | `1` or `0`                                            | Enables (1) or disables (0) font hinting.                                       |
|                          | `Xft.hintstyle` | `hintnone` / `hintslight` / `hintmedium` / `hintfull` | Controls the degree of font hinting.                                            |
|                          | `Xft.rgba`      | `none` / `rgb` / `bgr` / `vrgb` / `vbgr`              | Subpixel rendering order; depends on monitor pixel layout.                      |
|                          | `Xft.lcdfilter` | `lcdnone` / `lcddefault` / `lcdlight` / `lcdlegacy`   | LCD subpixel filtering mode.                                                    |

| **Xcursor (Mouse Cursor)** | `Xcursor.theme` | `breeze_cursors` / `Adwaita` / `DMZ-White` | Name of the cursor theme. |
| | `Xcursor.size` | `24` / `32` / `48` | Cursor size in pixels. |
| | `Xcursor.theme_core` | `true` / `false` | Whether to apply the theme to the core (legacy) cursor shapes. |

| **XTerm / URxvt (Terminal)** | `XTerm*faceName` / `URxvt.font` | `"Noto Sans Mono:size=10"` | Sets the font for terminal text. |
| | `XTerm*faceSize` / `URxvt.fontSize` | `10` | Font size. |
| | `XTerm*geometry` | `80x24` | Default terminal geometry. |
| | `XTerm*scrollBar` | `true` / `false` | Show or hide scroll bar. |
| | `URxvt*scrollBar_right` | `true` / `false` | Scrollbar on the right side. |
| | `URxvt*perl-ext-common` | `default,matcher` | List of enabled Perl extensions (URxvt only). |

| **GTK Theme (through xrdb bridge)** | `Gtk/FontName` | `"Noto Sans 10"` | Default GTK interface font. |
| | `Gtk/ThemeName` | `"Adwaita-dark"` | GTK theme name. |
| | `Gtk/IconThemeName` | `"breeze"` | GTK icon theme. |
| | `Gtk/CursorThemeName` | `"breeze_cursors"` | GTK cursor theme. |
| | `Gtk/CursorThemeSize` | `24` / `48` | Cursor size. |

| **General UI (X Toolkit apps)** | `*foreground` | `#ffffff` | Default text color. |
| | `*background` | `#000000` | Default background color. |
| | `*cursorColor` | `#ffcc00` | Text cursor color. |
| | `*highlightColor` | `#0078d7` | Highlight selection color. |
| | `*borderColor` | `#444444` | Border color for some widgets. |
| | `*font` | `fixed` / `9x15` / `xft:Noto Sans-10` | Fallback font setting for older X11 apps. |

| **Misc (Desktop & Toolkit Integration)** | `Net/ThemeName` | `"Breeze"` | GTK / DE theme name for toolkits integrating via XSettings. |
| | `Net/IconThemeName` | `"Papirus"` | Global icon theme. |
| | `Net/SoundThemeName` | `"freedesktop"` | Name of sound theme for event sounds. |
| | `Gdk/WindowScalingFactor` | `1` / `2` | Window scaling factor (used by GTK). |
| | `Gdk/UnscaledDPI` | `98304` | Unscaled base DPI (×1024 fixed point). |

## Notes on Value Calculation

| Concept                  | Explanation                                                                                                                 |
| ------------------------ | --------------------------------------------------------------------------------------------------------------------------- |
| `Xft.dpi`                | Usually calculated as: <br>`DPI = 25.4 × (screen_pixel_height / physical_height_mm)` <br>Example: 2160p on 15.6" (~141 dpi) |
| `Gdk/UnscaledDPI`        | GTK stores this as `DPI × 1024`. Example: `96 × 1024 = 98304`.                                                              |
| `Xft.hintstyle` & `rgba` | Correspond to fontconfig settings — usually set via `~/.config/fontconfig/fonts.conf` or inherited from DE.                 |
| `Xcursor.size`           | Often scaled according to `Gdk/WindowScalingFactor`.                                                                        |*/

class XResource : public AbstractSettings
{
    Q_OBJECT
public:
    enum XResourceKey {
        Unknown = 0,

        Xft_DPI,
        Xft_Antialias,
        Xft_Hinting,
        Xft_HintStyle,
        Xft_RGBA,
        Xft_LCDFilter,

        Xcursor_Theme,
        Xcursor_Size,
        Xcursor_ThemeCore,

        Gtk_FontName,
        Gtk_ThemeName,
        Gtk_IconThemeName,
        Gtk_CursorThemeName,
        Gtk_CursorThemeSize,

        Gdk_WindowScalingFactor,
        Gdk_UnscaledDPI,

        Net_ThemeName,
        Net_IconThemeName,
        Net_SoundThemeName
    };
    Q_ENUM(XResourceKey)

    explicit XResource(xcb_connection_t *connection, QObject *parent = nullptr);
    ~XResource() override;

    static QByteArray toByteArray(XResourceKey key);

    bool initialized() const override;
    bool isEmpty() const override;

    bool contains(const QByteArray &property) const override;
    QVariant getPropertyValue(const QByteArray &property) const override;
    void setPropertyValue(const QByteArray &property, const QVariant &value) override;
    QByteArrayList propertyList() const override;
    void apply() override;

private:
    void reload();

private:
    xcb_window_t m_root = XCB_WINDOW_NONE;
    QMap<QByteArray, QVariant> m_resources;
};
