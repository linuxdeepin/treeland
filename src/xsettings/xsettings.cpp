// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "xsettings.h"
#include "common/treelandlogging.h"

#include <QIODevice>
#include <QColor>
#include <QtEndian>

#define kMaxPropertySize 4096
#define XSETTINGS_ATOM_NAME "_XSETTINGS_SETTINGS"
#define MANAGER_ATOM_NAME "MANAGER"
#define XSETTINGS_NOTIFY_ATOM_NAME "_XSETTINGS_SETTINGS_NOTIFY"
#define XSETTINGS_SIGNAL_ATOM_NAME "_XSETTINGS_SETTINGS_SIGNAL"

static int round_to_nearest_multiple_of_4(int value)
{
    int remainder = value % 4;
    if (!remainder)
        return value;
    return value + 4 - remainder;
}

XSettings::XSettings(xcb_connection_t *connection, QObject *parent)
    : AbstractSettings(connection, parent)
{
    initX11(-1, true);
}

XSettings::~XSettings()
{
}

QByteArray XSettings::toByteArray(XSettingsKey key)
{
    switch (key) {
    case Xft_DPI: return QByteArrayLiteral("Xft/DPI");
    case Xft_Antialias: return QByteArrayLiteral("Xft/Antialias");
    case Xft_Hinting: return QByteArrayLiteral("Xft/Hinting");
    case Xft_HintStyle: return QByteArrayLiteral("Xft/HintStyle");
    case Xft_RGBA: return QByteArrayLiteral("Xft/RGBA");
    case Xft_LCDFilter: return QByteArrayLiteral("Xft/Lcdfilter");

    case Xcursor_Theme: return QByteArrayLiteral("Xcursor/Theme");
    case Xcursor_Size: return QByteArrayLiteral("Xcursor/Size");
    case Xcursor_ThemeCore: return QByteArrayLiteral("Xcursor/ThemeCore");

    case Gdk_WindowScalingFactor: return QByteArrayLiteral("Gdk/WindowScalingFactor");
    case Gdk_UnscaledDPI: return QByteArrayLiteral("Gdk/UnscaledDPI");

    case Gtk_FontName: return QByteArrayLiteral("Gtk/FontName");
    case Gtk_ThemeName: return QByteArrayLiteral("Gtk/ThemeName");
    case Gtk_IconThemeName: return QByteArrayLiteral("Gtk/IconThemeName");
    case Gtk_CursorThemeName: return QByteArrayLiteral("Gtk/CursorThemeName");
    case Gtk_CursorThemeSize: return QByteArrayLiteral("Gtk/CursorThemeSize");
    case Gtk_RecentFilesEnabled: return QByteArrayLiteral("Gtk/RecentFilesEnabled");
    case Gtk_ShowStatusShapes: return QByteArrayLiteral("Gtk/ShowStatusShapes");
    case Gtk_ShowInputMethodMenu: return QByteArrayLiteral("Gtk/ShowInputMethodMenu");
    case Gtk_TimeoutInitial: return QByteArrayLiteral("Gtk/TimeoutInitial");
    case Gtk_TimeoutRepeat: return QByteArrayLiteral("Gtk/TimeoutRepeat");
    case Gtk_DecorationLayout: return QByteArrayLiteral("Gtk/DecorationLayout");
    case Gtk_IMModule: return QByteArrayLiteral("Gtk/IMModule");
    case Gtk_ShellShowsDesktop: return QByteArrayLiteral("Gtk/ShellShowsDesktop");
    case Gtk_MenuImages: return QByteArrayLiteral("Gtk/MenuImages");
    case Gtk_EnablePrimaryPaste: return QByteArrayLiteral("Gtk/EnablePrimaryPaste");
    case Gtk_KeynavUseCaret: return QByteArrayLiteral("Gtk/KeynavUseCaret");
    case Gtk_ShellShowsAppMenu: return QByteArrayLiteral("Gtk/ShellShowsAppMenu");
    case Gtk_CanChangeAccels: return QByteArrayLiteral("Gtk/CanChangeAccels");
    case Gtk_DialogsUseHeader: return QByteArrayLiteral("Gtk/DialogsUseHeader");
    case Gtk_ToolbarStyle: return QByteArrayLiteral("Gtk/ToolbarStyle");
    case Gtk_KeyThemeName: return QByteArrayLiteral("Gtk/KeyThemeName");
    case Gtk_IMPreeditStyle: return QByteArrayLiteral("Gtk/IMPreeditStyle");
    case Gtk_EnableAnimations: return QByteArrayLiteral("Gtk/EnableAnimations");
    case Gtk_ToolbarIconSize: return QByteArrayLiteral("Gtk/ToolbarIconSize");
    case Gtk_IMStatusStyle: return QByteArrayLiteral("Gtk/IMStatusStyle");
    case Gtk_RecentFilesMaxAge: return QByteArrayLiteral("Gtk/RecentFilesMaxAge");
    case Gtk_Modules: return QByteArrayLiteral("Gtk/Modules");
    case Gtk_AutoMnemonics: return QByteArrayLiteral("Gtk/AutoMnemonics");
    case Gtk_ColorScheme: return QByteArrayLiteral("Gtk/ColorScheme");
    case Gtk_MenuBarAccel: return QByteArrayLiteral("Gtk/MenuBarAccel");
    case Gtk_ColorPalette: return QByteArrayLiteral("Gtk/ColorPalette");
    case Gtk_OverlayScrolling: return QByteArrayLiteral("Gtk/OverlayScrolling");
    case Gtk_SessionBusId: return QByteArrayLiteral("Gtk/SessionBusId");
    case Gtk_ShowUnicodeMenu: return QByteArrayLiteral("Gtk/ShowUnicodeMenu");
    case Gtk_CursorBlinkTimeout: return QByteArrayLiteral("Gtk/CursorBlinkTimeout");
    case Gtk_ButtonImages: return QByteArrayLiteral("Gtk/ButtonImages");
    case Gtk_TitlebarRightClick: return QByteArrayLiteral("Gtk/TitlebarRightClick");
    case Gtk_TitlebarDoubleClick: return QByteArrayLiteral("Gtk/TitlebarDoubleClick");
    case Gtk_TitlebarMiddleClick: return QByteArrayLiteral("Gtk/TitlebarMiddleClick");
    case Gtk_MonospaceFontName: return QByteArrayLiteral("Gtk/MonospaceFontName");
    case Gtk_ApplicationPreferDarkTheme: return QByteArrayLiteral("Gtk/ApplicationPreferDarkTheme");
    case Gtk_PrimaryButtonWarpsSlider: return QByteArrayLiteral("Gtk/PrimaryButtonWarpsSlider");

    case Net_DndDragThreshold: return QByteArrayLiteral("Net/DndDragThreshold");
    case Net_CursorBlinkTime: return QByteArrayLiteral("Net/CursorBlinkTime");
    case Net_ThemeName: return QByteArrayLiteral("Net/ThemeName");
    case Net_DoubleClickTime: return QByteArrayLiteral("Net/DoubleClickTime");
    case Net_CursorBlink: return QByteArrayLiteral("Net/CursorBlink");
    case Net_FallbackIconTheme: return QByteArrayLiteral("Net/FallbackIconTheme");
    case Net_EnableEventSounds: return QByteArrayLiteral("Net/EnableEventSounds");
    case Net_IconThemeName: return QByteArrayLiteral("Net/IconThemeName");
    case Net_SoundThemeName: return QByteArrayLiteral("Net/SoundThemeName");
    case Net_EnableInputFeedbackSounds: return QByteArrayLiteral("Net/EnableInputFeedbackSounds");
    case Net_PreferDarkTheme: return QByteArrayLiteral("Net/PreferDarkTheme");

    default: return QByteArrayLiteral("");
    }
}

bool XSettings::initialized() const
{
    return !m_windows.empty();
}

bool XSettings::isEmpty() const
{
    return m_settings.empty();
}

bool XSettings::contains(const QByteArray &property) const
{
    return m_settings.contains(property);
}

QVariant XSettings::getPropertyValue(const QByteArray &property) const
{
    auto it = m_settings.constFind(property);
    if (it == m_settings.constEnd())
        return QVariant();

    return it->value;
}

void XSettings::setPropertyValue(const QByteArray &property, const QVariant &value)
{
    XSettingsPropertyValue &xvalue = m_settings[property];
    if (xvalue.value == value)
        return;

    if (!value.isValid())
        return;

    xvalue.updateValue(value, xvalue.last_change_serial + 1);
    ++m_serial;
}

QByteArray XSettings::depopulateSettings()
{
    QByteArray xSettings;
    uint number_of_settings = m_settings.size();
    xSettings.reserve(12 + number_of_settings * 12);
    char byteOrder = QSysInfo::ByteOrder == QSysInfo::LittleEndian ? XCB_IMAGE_ORDER_LSB_FIRST : XCB_IMAGE_ORDER_MSB_FIRST;

    xSettings.append(byteOrder); //byte-order
    xSettings.append(3, '\0'); //unused
    xSettings.append((char*)&m_serial, sizeof(m_serial)); //SERIAL
    xSettings.append((char*)&number_of_settings, sizeof(number_of_settings)); //N_SETTINGS
    uint number_of_settings_index = xSettings.size() - sizeof(number_of_settings);
    for (auto i = m_settings.constBegin(); i != m_settings.constEnd(); ++i) {
        const XSettingsPropertyValue &value = i.value();
        if (!value.value.isValid()) {
            --number_of_settings;
            continue;
        }

        char type = XSettingsTypeString;
        const QByteArray &key = i.key();
        quint16 key_size = key.size();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        switch (value.value.typeId()) {
#else
        switch (value.value.type()) {
#endif
        case QMetaType::QColor:
            type = XSettingsTypeColor;
            break;
        case QMetaType::Int:
        case QMetaType::Bool:
            type = XSettingsTypeInteger;
            break;
        default:
            break;
        }

        xSettings.append(type); //type
        xSettings.append('\0'); //unused
        xSettings.append((char*)&key_size, 2); //name-len
        xSettings.append(key.constData()); //name
        xSettings.append(3 - (key_size + 3) % 4, '\0'); // 4-byte alignment
        xSettings.append((char*)&value.last_change_serial, 4); //last-change-serial

        QByteArray value_data;

        if (type == XSettingsTypeInteger) {
            qint32 int_value = value.value.toInt();
            value_data.append((char*)&int_value, 4);
        } else if (type == XSettingsTypeColor) {
            const QColor &color = qvariant_cast<QColor>(value.value);
            quint16 red = color.red();
            quint16 green = color.green();
            quint16 blue = color.blue();
            quint16 alpha = color.alpha();

            value_data.append((char*)&red, 2);
            value_data.append((char*)&green, 2);
            value_data.append((char*)&blue, 2);
            value_data.append((char*)&alpha, 2);
        } else {
            const QByteArray &string_data = value.value.toByteArray();
            quint32 data_size = string_data.size();
            value_data.append((char*)&data_size, 4);
            value_data.append(string_data);
            value_data.append(3 - (string_data.size() + 3) % 4, '\0'); // 4-byte alignment
        }

        xSettings.append(value_data);
    }

    if (number_of_settings == 0) {
        return QByteArray();
    }
    memcpy(xSettings.data() + number_of_settings_index, &number_of_settings, sizeof(number_of_settings));

    return xSettings;
}

void XSettings::populateSettings(const QByteArray &xSettings)
{
    if (xSettings.length() < 12)
        return;
    char byteOrder = xSettings.at(0);
    if (byteOrder != XCB_IMAGE_ORDER_LSB_FIRST && byteOrder != XCB_IMAGE_ORDER_MSB_FIRST) {
        return;
    }

#define ADJUST_BO(b, t, x) \
    ((b == XCB_IMAGE_ORDER_LSB_FIRST) ?                          \
         qFromLittleEndian<t>(x) : \
         qFromBigEndian<t>(x))
#define VALIDATE_LENGTH(x)    \
        if ((size_t)xSettings.length() < (offset + local_offset + 12 + x)) { \
            return;                                                     \
    }

    m_serial = ADJUST_BO(byteOrder, qint32, xSettings.mid(4,4).constData());
    uint number_of_settings = ADJUST_BO(byteOrder, quint32, xSettings.mid(8,4).constData());
    const char *data = xSettings.constData() + 12;
    size_t offset = 0;
    QSet<QByteArray> keys;
    keys.reserve(number_of_settings);

    for (uint i = 0; i < number_of_settings; i++) {
        int local_offset = 0;
        VALIDATE_LENGTH(2);
        XSettingsType type = static_cast<XSettingsType>(*reinterpret_cast<const quint8 *>(data + offset));
        local_offset += 2;

        VALIDATE_LENGTH(2);
        quint16 name_len = ADJUST_BO(byteOrder, quint16, data + offset + local_offset);
        local_offset += 2;

        VALIDATE_LENGTH(name_len);
        QByteArray name(data + offset + local_offset, name_len);
        local_offset += round_to_nearest_multiple_of_4(name_len);

        VALIDATE_LENGTH(4);
        int last_change_serial = ADJUST_BO(byteOrder, qint32, data + offset + local_offset);
        Q_UNUSED(last_change_serial);
        local_offset += 4;

        QVariant value;
        if (type == XSettingsTypeString) {
            VALIDATE_LENGTH(4);
            int value_length = ADJUST_BO(byteOrder, qint32, data + offset + local_offset);
            local_offset+=4;
            VALIDATE_LENGTH(value_length);
            QByteArray value_string(data + offset + local_offset, value_length);
            value.setValue(value_string);
            local_offset += round_to_nearest_multiple_of_4(value_length);
        } else if (type == XSettingsTypeInteger) {
            VALIDATE_LENGTH(4);
            int value_length = ADJUST_BO(byteOrder, qint32, data + offset + local_offset);
            local_offset += 4;
            value.setValue(value_length);
        } else if (type == XSettingsTypeColor) {
            VALIDATE_LENGTH(2*4);
            quint16 red = ADJUST_BO(byteOrder, quint16, data + offset + local_offset);
            local_offset += 2;
            quint16 green = ADJUST_BO(byteOrder, quint16, data + offset + local_offset);
            local_offset += 2;
            quint16 blue = ADJUST_BO(byteOrder, quint16, data + offset + local_offset);
            local_offset += 2;
            quint16 alpha= ADJUST_BO(byteOrder, quint16, data + offset + local_offset);
            local_offset += 2;
            QColor color_value(red,green,blue,alpha);
            value.setValue(color_value);
        }
        offset += local_offset;

        m_settings[name].updateValue(value,last_change_serial);
        keys << name;
    }

    for (const QByteArray &key : m_settings.keys()) {
        if (!keys.contains(key)) {
            m_settings[key].updateValue(QVariant(), INT_MAX);
            m_settings.remove(key);
        }
    }
}

void XSettings::setSettings(const QByteArray &data)
{
    xcb_grab_server(m_connection);

    foreach (xcb_window_t win, m_windows) {
        xcb_change_property(m_connection,
                            XCB_PROP_MODE_REPLACE,
                            win,
                            m_atom,
                            m_atom,
                            8, data.size(), data.constData());

        if (win) {
            xcb_client_message_event_t notify_event;
            memset(&notify_event, 0, sizeof(notify_event));

            notify_event.response_type = XCB_CLIENT_MESSAGE;
            notify_event.format = 32;
            notify_event.sequence = 0;
            notify_event.window = win;
            notify_event.type = m_notifyAtom;
            notify_event.data.data32[0] = win;
            notify_event.data.data32[1] = m_atom;

            xcb_send_event(m_connection, false, win, XCB_EVENT_MASK_PROPERTY_CHANGE, (const char *)&notify_event);
        }
    }
    xcb_ungrab_server(m_connection);
}

QByteArrayList XSettings::propertyList() const
{
    QByteArrayList merged;
    for (auto v : m_settings.keys())
        merged.append(v);

    return merged;
}

void XSettings::apply()
{
    setSettings(depopulateSettings());
}

bool XSettings::initX11(int screen, bool replace) {
    xcb_intern_atom_cookie_t atomCookie =
        xcb_intern_atom(m_connection, 0, strlen(XSETTINGS_ATOM_NAME), XSETTINGS_ATOM_NAME);
    xcb_intern_atom_reply_t *atomReply = xcb_intern_atom_reply(m_connection, atomCookie, nullptr);

    if (!atomReply) {
        qCCritical(treelandXsettings) << "xcb_intern_atom_reply return nullptr for" << XSETTINGS_ATOM_NAME;
        return false;
    }

    m_atom = atomReply->atom;
    free(atomReply);
    Q_ASSERT(m_atom != XCB_NONE);

    xcb_intern_atom_cookie_t notifyAtomCookie =
        xcb_intern_atom(m_connection, 0, strlen(XSETTINGS_NOTIFY_ATOM_NAME), XSETTINGS_NOTIFY_ATOM_NAME);
    xcb_intern_atom_reply_t *notifyAtomReply = xcb_intern_atom_reply(m_connection, notifyAtomCookie, nullptr);

    if (!notifyAtomReply) {
        qCCritical(treelandXsettings) << "xcb_intern_atom_reply return nullptr for" << XSETTINGS_NOTIFY_ATOM_NAME;
        return false;
    }

    m_notifyAtom = notifyAtomReply->atom;
    free(notifyAtomReply);
    Q_ASSERT(m_notifyAtom != XCB_NONE);

    xcb_intern_atom_cookie_t signalAtomCookie =
        xcb_intern_atom(m_connection, 0, strlen(XSETTINGS_SIGNAL_ATOM_NAME), XSETTINGS_SIGNAL_ATOM_NAME);
    xcb_intern_atom_reply_t *signalAtomReply = xcb_intern_atom_reply(m_connection, signalAtomCookie, nullptr);

    if (!signalAtomReply) {
        qCCritical(treelandXsettings) << "xcb_intern_atom_reply return nullptr for" << XSETTINGS_SIGNAL_ATOM_NAME;
        return false;
    }

    m_signalAtom = signalAtomReply->atom;
    free(signalAtomReply);
    Q_ASSERT(m_signalAtom != XCB_NONE);

    char data[kMaxPropertySize] = {0};
    size_t bytesWritten = 128;

    const xcb_setup_t *setup = xcb_get_setup(m_connection);
    int screen_count = xcb_setup_roots_length(setup);

    int min_screen = 0;
    int max_screen = screen_count - 1;
    if (screen >= 0)
        min_screen = max_screen = screen;

    for (int s = min_screen; s <= max_screen; ++s) {
        xcb_window_t win;
        xcb_timestamp_t timestamp;
        if (!createWindow(s, &win, &timestamp)) {
            qCCritical(treelandXsettings) << "xsettingsd Unable to create window on screen" << s;
            return false;
        }

        xcb_change_property(m_connection,
                            XCB_PROP_MODE_REPLACE,
                            win,
                            m_atom,
                            m_atom,
                            8,
                            bytesWritten,
                            data);
        if (!manageScreen(s, win, timestamp, replace))
            return false;

        m_windows.push_back(win);
        qCDebug(treelandXsettings) << "XSettings: registered window" << win << "for screen" << s;
    }

    xcb_flush(m_connection);
    return true;
}

bool XSettings::createWindow(int screen, xcb_window_t *out_win, xcb_timestamp_t *out_time) {
    const xcb_setup_t *setup = xcb_get_setup(m_connection);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen; ++i)
        xcb_screen_next(&it);

    xcb_screen_t *scr = it.data;
    if (!scr) {
        qCCritical(treelandXsettings) << "xcb_setup_roots_iterator failed";
        return false;
    }

    xcb_window_t win = xcb_generate_id(m_connection);
    uint32_t values[] = { XCB_EVENT_MASK_PROPERTY_CHANGE };

    xcb_create_window(m_connection,
                      XCB_COPY_FROM_PARENT,
                      win,
                      scr->root,
                      -1, -1, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      scr->root_visual,
                      XCB_CW_EVENT_MASK,
                      values);

    const char *name = "xsettings-manager";
    xcb_change_property(m_connection,
                        XCB_PROP_MODE_REPLACE,
                        win,
                        XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING,
                        8,
                        strlen(name),
                        name);

    *out_win = win;
    *out_time = XCB_CURRENT_TIME;

    qCDebug(treelandXsettings) << "Created XSETTINGS window" << win << "named" << name;

    return true;
}

bool XSettings::manageScreen(int screen, xcb_window_t win, xcb_timestamp_t timestamp, bool replace) {
    char sel_name[64];
    snprintf(sel_name, sizeof(sel_name), "_XSETTINGS_S%d", screen);

    xcb_intern_atom_cookie_t sel_cookie =
        xcb_intern_atom(m_connection, 0, strlen(sel_name), sel_name);
    xcb_intern_atom_reply_t *sel_reply = xcb_intern_atom_reply(m_connection, sel_cookie, nullptr);
    if (!sel_reply) {
        qCCritical(treelandXsettings)
        << "xcb_intern_atom_reply return nullptr for"
        << QString("_XSETTINGS_S%1").arg(screen);

        return false;
    }

    xcb_atom_t selection_atom = sel_reply->atom;
    free(sel_reply);

    xcb_grab_server(m_connection);
    xcb_get_selection_owner_cookie_t owner_cookie = xcb_get_selection_owner(m_connection, selection_atom);
    xcb_get_selection_owner_reply_t *owner_reply = xcb_get_selection_owner_reply(m_connection, owner_cookie, nullptr);
    xcb_window_t owner = XCB_NONE;
    if (owner_reply) {
        owner = owner_reply->owner;
        free(owner_reply);
    }

    if (owner != XCB_NONE && !replace) {
        xcb_ungrab_server(m_connection);
        qCCritical(treelandXsettings)
            << "xsettings: Another XSETTINGS manager exists for screen"
            << screen
            << "owner window" << owner;
        return false;
    }

    xcb_set_selection_owner(m_connection, win, selection_atom, timestamp);

    xcb_get_selection_owner_reply_t *reply =
        xcb_get_selection_owner_reply(m_connection,
                                      xcb_get_selection_owner(m_connection, selection_atom),
                                      nullptr);
    bool ok = reply && reply->owner == win;
    free(reply);
    xcb_ungrab_server(m_connection);

    if (!ok) {
        qCCritical(treelandXsettings) << "xsettingsd: Failed to acquire ownership of" << sel_name;
        return false;
    }

    const xcb_setup_t *setup = xcb_get_setup(m_connection);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen; ++i)
        xcb_screen_next(&it);
    xcb_window_t root = it.data->root;
    xcb_intern_atom_cookie_t man_cookie =
        xcb_intern_atom(m_connection, 0, strlen(MANAGER_ATOM_NAME), MANAGER_ATOM_NAME);
    xcb_intern_atom_reply_t *man_reply =
        xcb_intern_atom_reply(m_connection, man_cookie, nullptr);
    if (!man_reply) {
        qCCritical(treelandXsettings) << "Failed to intern MANAGER atom";
        return false;
    }

    xcb_atom_t manager_atom = man_reply->atom;
    free(man_reply);

    xcb_client_message_event_t ev = {};
    ev.response_type = XCB_CLIENT_MESSAGE;
    ev.window = root;
    ev.type = manager_atom;
    ev.format = 32;
    ev.data.data32[0] = timestamp;
    ev.data.data32[1] = selection_atom;
    ev.data.data32[2] = win;
    ev.data.data32[3] = 0;
    ev.data.data32[4] = 0;

    xcb_send_event(m_connection,
                   false,
                   root,
                   XCB_EVENT_MASK_STRUCTURE_NOTIFY,
                   reinterpret_cast<const char *>(&ev));
    xcb_flush(m_connection);

    return true;
}
