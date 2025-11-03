// Copyright (C) 2025 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "xresource.h"
#include "common/treelandlogging.h"

#define XRESOURCE_ATOM_NAME "RESOURCE_MANAGER"

static QPair<QByteArray, QByteArray> splitXResourceLine(const QByteArray &line)
{
    int pos = -1;
    bool escaped = false;

    for (int i = 0; i < line.size(); ++i) {
        if (line[i] == '\\') {
            escaped = !escaped;
        } else if (line[i] == ':' && !escaped) {
            pos = i;
            break;
        } else {
            escaped = false;
        }
    }

    if (pos == -1)
        return {line.trimmed(), QByteArray()};

    QByteArray key = line.left(pos).trimmed();
    QByteArray value = line.mid(pos + 1).trimmed();
    value.replace("\\:", ":");

    return {key, value};
}

XResource::XResource(xcb_connection_t *connection, QObject *parent)
    : AbstractSettings(connection, parent)
{
    const xcb_setup_t *setup = xcb_get_setup(m_connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    m_root = iter.data->root;
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(m_connection, 0, strlen(XRESOURCE_ATOM_NAME), XRESOURCE_ATOM_NAME);
    xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply(m_connection, cookie, nullptr);
    if (reply) {
        m_atom = reply->atom;
    }

    free(reply);
}

XResource::~XResource()
{
}

QByteArray XResource::toByteArray(XResourceKey key)
{
    switch (key) {
    case Xft_DPI: return QByteArrayLiteral("Xft.dpi");
    case Xft_Antialias: return QByteArrayLiteral("Xft.antialias");
    case Xft_Hinting: return QByteArrayLiteral("Xft.hinting");
    case Xft_HintStyle: return QByteArrayLiteral("Xft.hintstyle");
    case Xft_RGBA: return QByteArrayLiteral("Xft.rgba");
    case Xft_LCDFilter: return QByteArrayLiteral("Xft.lcdfilter");
    case Xcursor_Theme: return QByteArrayLiteral("Xcursor.theme");
    case Xcursor_Size: return QByteArrayLiteral("Xcursor.size");
    case Xcursor_ThemeCore: return QByteArrayLiteral("Xcursor.theme_core");
    case Gtk_FontName: return QByteArrayLiteral("Gtk/FontName");
    case Gtk_ThemeName: return QByteArrayLiteral("Gtk/ThemeName");
    case Gtk_IconThemeName: return QByteArrayLiteral("Gtk/IconThemeName");
    case Gtk_CursorThemeName: return QByteArrayLiteral("Gtk/CursorThemeName");
    case Gtk_CursorThemeSize: return QByteArrayLiteral("Gtk/CursorThemeSize");
    case Gdk_WindowScalingFactor: return QByteArrayLiteral("Gdk/WindowScalingFactor");
    case Gdk_UnscaledDPI: return QByteArrayLiteral("Gdk/UnscaledDPI");
    case Net_ThemeName: return QByteArrayLiteral("Net/ThemeName");
    case Net_IconThemeName: return QByteArrayLiteral("Net/IconThemeName");
    case Net_SoundThemeName: return QByteArrayLiteral("Net/SoundThemeName");
    default: return QByteArrayLiteral("");
    }
}

bool XResource::initialized() const
{
    return true;
}

bool XResource::isEmpty() const
{
    return m_resources.isEmpty();
}

bool XResource::contains(const QByteArray &property) const
{
    return m_resources.contains(property);
}

QVariant XResource::getPropertyValue(const QByteArray &property) const
{
    auto it = m_resources.constFind(property);
    return it != m_resources.constEnd() ? it.value() : QVariant();
}

void XResource::setPropertyValue(const QByteArray &property, const QVariant &value)
{
    QVariant &xvalue = m_resources[property];
    if (xvalue == value)
        return;

    if (!value.isValid())
        return;

    m_resources[property] = value;
}

QByteArrayList XResource::propertyList() const
{
    QByteArrayList merged;
    for (auto v : m_resources.keys())
        merged.append(v);

    return merged;
}

void XResource::apply()
{
    QByteArray text;
    for (auto it = m_resources.constBegin(); it != m_resources.constEnd(); ++it) {
        text.append(it.key());
        text.append(": ");
        text.append(it.value().toString().toUtf8());
        text.append('\n');
    }

    xcb_change_property(m_connection,
                        XCB_PROP_MODE_REPLACE,
                        m_root,
                        m_atom,
                        XCB_ATOM_STRING,
                        8,
                        text.size(),
                        text.constData());
    xcb_flush(m_connection);
}

void XResource::reload()
{
    m_resources.clear();

    xcb_get_property_cookie_t cookie =
        xcb_get_property(m_connection, 0, m_root, m_atom,
                         XCB_ATOM_STRING, 0, UINT32_MAX);
    xcb_get_property_reply_t *reply =
        xcb_get_property_reply(m_connection, cookie, nullptr);
    if (!reply) {
        qCCritical(treelandXsettings) << "xcb_intern_atom_reply returned nullptr, atom:" << XRESOURCE_ATOM_NAME;
        return;
    }

    int len = xcb_get_property_value_length(reply);
    const char *data = (const char *)xcb_get_property_value(reply);
    QByteArray text(data, len);
    free(reply);

    const QList<QByteArray> lines = text.split('\n');
    for (const QByteArray &line : std::as_const(lines)) {
        if (line.trimmed().isEmpty())
            continue;

        auto [key, value] = splitXResourceLine(line);
        if (!key.isEmpty())
            m_resources[key] = QString::fromUtf8(value);
    }
}
