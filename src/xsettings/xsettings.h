// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include "abstractsettings.h"

class XSettingsPropertyValue
{
public:
    XSettingsPropertyValue()
    {}

    bool updateValue(const QVariant &value, int last_change_serial)
    {
        if (last_change_serial <= this->last_change_serial)
            return false;
        this->value = value;
        this->last_change_serial = last_change_serial;

        return true;
    }

    QVariant value;
    int last_change_serial = -1;
};

/*## **Gtk (Graphical Toolkit) Settings**
| **Property**              | **Example Value**                       | **Meaning / Purpose (for comments)**                                     |
| ------------------------- | --------------------------------------- | ------------------------------------------------------------------------ |
| `Gdk/UnscaledDPI`         | `98304`           | Physical display DPI ×1024, before scaling. Used internally by GDK to compute display density. |
| `Gdk/WindowScalingFactor` | `2`               | Global scaling factor for HiDPI screens (integer). Affects window buffer scaling in GDK.       |
| `Gtk/RecentFilesEnabled`  | `1`                                     | Enables “recent files” feature in GTK applications.                      |
| `Gtk/ShowStatusShapes`    | `0`                                     | Whether to display shapes or indicators in status icons.                 |
| `Gtk/ShowInputMethodMenu` | `0`                                     | Controls visibility of the input method (IME) menu in text widgets.      |
| `Gtk/TimeoutInitial`      | `200`                                   | Keyboard repeat initial delay (milliseconds).                            |
| `Gtk/TimeoutRepeat`       | `20`                                    | Keyboard repeat rate (milliseconds between repeats).                     |
| `Gtk/DecorationLayout`    | `":minimize,maximize,close"`            | Titlebar button order (client-side decorations).                         |
| `Gtk/IMModule`            | `"ibus"`                                | Input method module (e.g., ibus, fcitx, xim).                            |
| `Gtk/ShellShowsDesktop`   | `0`                                     | Whether the desktop shell displays the desktop.                          |
| `Gtk/MenuImages`          | `0`                                     | Show or hide icons in application menus.                                 |
| `Gtk/EnablePrimaryPaste`  | `1`                                     | Enables middle-click paste using PRIMARY selection buffer.               |
| `Gtk/KeynavUseCaret`      | `0`                                     | Enables caret navigation in widgets via keyboard.                        |
| `Gtk/ShellShowsAppMenu`   | `0`                                     | Whether the global app menu is displayed by shell.                       |
| `Gtk/CanChangeAccels`     | `0`                                     | Allows users to change keyboard shortcuts at runtime.                    |
| `Gtk/FontName`            | `"Noto Sans, 10"`                       | Default UI font for GTK widgets.                                         |
| `Gtk/CursorThemeSize`     | `48`                                    | Size of cursor icons (pixels).                                           |
| `Gtk/DialogsUseHeader`    | `1`                                     | Use modern client-side header bars in dialogs.                           |
| `Gtk/ToolbarStyle`        | `"both-horiz"`                          | Toolbar layout: icons, text, or both.                                    |
| `Gtk/KeyThemeName`        | `"Default"`                             | Keybinding theme (e.g., “Emacs”).                                        |
| `Gtk/IMPreeditStyle`      | `"callback"`                            | Preedit (composition) display style for IME input.                       |
| `Gtk/EnableAnimations`    | `1`                                     | Enables widget animations.                                               |
| `Gtk/CursorThemeName`     | `"breeze_cursors"`                      | Cursor icon theme.                                                       |
| `Gtk/ToolbarIconSize`     | `"large"`                               | Default toolbar icon size.                                               |
| `Gtk/IMStatusStyle`       | `"callback"`                            | IME status display style.                                                |
| `Gtk/RecentFilesMaxAge`   | `-1`                                    | Max age (in days) for items in “recent files”; `-1` disables expiration. |
| `Gtk/Modules`             | `"canberra-gtk-module:gail:atk-bridge"` | List of GTK modules to load (sound, accessibility, etc.).                |
| `Gtk/AutoMnemonics`       | `1`                                     | Enables automatic mnemonics (Alt shortcuts).                             |
| `Gtk/ColorScheme`         | `""`                                    | Deprecated; used for color overrides.                                    |
| `Gtk/MenuBarAccel`        | `"F10"`                                 | Keyboard shortcut to focus the menu bar.                                 |
| `Gtk/ColorPalette`        | `"black:white:gray50:..."`              | Palette used in color selection dialogs.                                 |
| `Gtk/OverlayScrolling`    | `1`                                     | Enables overlay scrollbars.                                              |
| `Gtk/SessionBusId`        | `"1412549d..."`                         | D-Bus session bus ID (auto-generated).                                   |
| `Gtk/ShowUnicodeMenu`     | `0`                                     | Enables Unicode character input menu in text widgets.                    |
| `Gtk/CursorBlinkTimeout`  | `10`                                    | Timeout before text cursor stops blinking (seconds).                     |
| `Gtk/ButtonImages`        | `0`                                     | Show icons on buttons.                                                   |
| `Gtk/TitlebarRightClick`  | `"menu"`                                | Defines action when right-clicking titlebar (e.g., show menu).           |
| `Gtk/TitlebarDoubleClick` | `"toggle-maximize"`                     | Action for double-click on titlebar.                                     |
| `Gtk/TitlebarMiddleClick` | `"none"`                                | Action for middle-click on titlebar.                                     |
| `Gtk/ThemeName`           |                                         | Explicit GTK widget theme (distinct from `Net/ThemeName`).               |
| `Gtk/MonospaceFontName`   |                                         | Font for monospaced text (terminals, editors).                           |
| `Gtk/ApplicationPreferDarkTheme` | `0`                              | App-specific dark theme preference. 0 Light theme，1 Dark theme          |
| `Gtk/PrimaryButtonWarpsSlider`   | `0`                              | Determines if primary click warps scrollbar thumb.                       |

## **Net (Freedesktop Desktop Interoperability) Settings**

| **Property**                    | **Example Value** | **Meaning / Purpose (for comments)**                           |
| ------------------------------- | ----------------- | -------------------------------------------------------------- |
| `Net/DndDragThreshold`          | `8`               | Pixel distance threshold before drag-and-drop begins.          |
| `Net/CursorBlinkTime`           | `1000`            | Blink interval of text cursor (milliseconds).                  |
| `Net/ThemeName`                 | `"deepin"`        | Current GTK/desktop theme name.                                |
| `Net/DoubleClickTime`           | `400`             | Maximum time between clicks for double-click recognition (ms). |
| `Net/CursorBlink`               | `1`               | Whether the text cursor blinks.                                |
| `Net/FallbackIconTheme`         | `"gnome"`         | Fallback icon theme when main one is missing icons.            |
| `Net/EnableEventSounds`         | `1`               | Enables UI event sounds (button click, notifications, etc.).   |
| `Net/IconThemeName`             | `"breeze"`        | Icon theme used for GTK applications.                          |
| `Net/SoundThemeName`            | `"__custom"`      | Sound theme name for system sounds.                            |
| `Net/EnableInputFeedbackSounds` | `0`               | Enables keypress feedback sounds.                              |
| `Net/PreferDarkTheme`           |                   | Indicates preference for dark theme variants.                  |

## **Xft (X FreeType) Font Settings**

| **Property**    | **Example Value** | **Meaning / Purpose (for comments)**                                |
| --------------- | ----------------- | ------------------------------------------------------------------- |
| `Xft/Antialias` | `1`               | Enables anti-aliased font rendering.                                |
| `Xft/Hinting`   | `1`               | Enables font hinting.                                               |
| `Xft/HintStyle` | `"hintslight"`    | Hinting level (`hintnone`, `hintslight`, `hintmedium`, `hintfull`). |
| `Xft/DPI`       | `196608`          | Effective DPI (×1024). Controls font scaling.                       |
| `Xft/RGBA`      | `"none"`          | Subpixel rendering type (`rgb`, `bgr`, `vrgb`, `vbgr`, `none`).     |
| `Xft/Lcdfilter` |                   | LCD filtering method (`lcddefault`, `lcdlight`, etc.) for subpixel rendering. |*/

class XSettings : public AbstractSettings
{
    Q_OBJECT
public:
    enum XSettingsKey {
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

        Gdk_WindowScalingFactor,
        Gdk_UnscaledDPI,

        Gtk_FontName,
        Gtk_ThemeName,
        Gtk_IconThemeName,
        Gtk_CursorThemeName,
        Gtk_CursorThemeSize,
        Gtk_RecentFilesEnabled,
        Gtk_ShowStatusShapes,
        Gtk_ShowInputMethodMenu,
        Gtk_TimeoutInitial,
        Gtk_TimeoutRepeat,
        Gtk_DecorationLayout,
        Gtk_IMModule,
        Gtk_ShellShowsDesktop,
        Gtk_MenuImages,
        Gtk_EnablePrimaryPaste,
        Gtk_KeynavUseCaret,
        Gtk_ShellShowsAppMenu,
        Gtk_CanChangeAccels,
        Gtk_DialogsUseHeader,
        Gtk_ToolbarStyle,
        Gtk_KeyThemeName,
        Gtk_IMPreeditStyle,
        Gtk_EnableAnimations,
        Gtk_ToolbarIconSize,
        Gtk_IMStatusStyle,
        Gtk_RecentFilesMaxAge,
        Gtk_Modules,
        Gtk_AutoMnemonics,
        Gtk_ColorScheme,
        Gtk_MenuBarAccel,
        Gtk_ColorPalette,
        Gtk_OverlayScrolling,
        Gtk_SessionBusId,
        Gtk_ShowUnicodeMenu,
        Gtk_CursorBlinkTimeout,
        Gtk_ButtonImages,
        Gtk_TitlebarRightClick,
        Gtk_TitlebarDoubleClick,
        Gtk_TitlebarMiddleClick,
        Gtk_MonospaceFontName,
        Gtk_ApplicationPreferDarkTheme,
        Gtk_PrimaryButtonWarpsSlider,

        Net_DndDragThreshold,
        Net_CursorBlinkTime,
        Net_ThemeName,
        Net_DoubleClickTime,
        Net_CursorBlink,
        Net_FallbackIconTheme,
        Net_EnableEventSounds,
        Net_IconThemeName,
        Net_SoundThemeName,
        Net_EnableInputFeedbackSounds,
        Net_PreferDarkTheme
    };
    Q_ENUM(XSettingsKey)

    enum XSettingsType {
        XSettingsTypeInteger = 0,
        XSettingsTypeString,
        XSettingsTypeColor
    };
    Q_ENUM(XSettingsType)

    explicit XSettings(xcb_connection_t *connection, QObject *parent = nullptr);
    ~XSettings() override;

    static QByteArray toByteArray(XSettingsKey key);

    bool initialized() const override;
    bool isEmpty() const override;

    bool contains(const QByteArray &property) const override;
    QVariant getPropertyValue(const QByteArray &property) const override;
    void setPropertyValue(const QByteArray &property, const QVariant &value) override;
    QByteArrayList propertyList() const override;
    void apply() override;

private:
    bool initX11(int screen, bool replace);
    bool createWindow(int screen, xcb_window_t *out_win, xcb_timestamp_t *out_time);
    bool manageScreen(int screen, xcb_window_t win, xcb_timestamp_t timestamp, bool replace);
    QByteArray depopulateSettings();
    void populateSettings(const QByteArray &xSettings);
    void setSettings(const QByteArray &data);

private:
    std::vector<xcb_window_t> m_windows;
    QMap<QByteArray, XSettingsPropertyValue> m_settings;
    xcb_atom_t m_notifyAtom = XCB_NONE;
    xcb_atom_t m_signalAtom = XCB_NONE;
    int m_serial = -1;
};
