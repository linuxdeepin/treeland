// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "modules/personalization/personalizationmanagerinterfacev1.h"
#include "server-bridge.h"
#include "seat/helper.h"
#include "treeland-personalization-manager-v1.h"
#include "treelanduserconfig.hpp"

#include <wserver.h>

#include <cstring>

WAYLIB_SERVER_USE_NAMESPACE

namespace {
PersonalizationWindowContextV1 *g_windowContext = nullptr;

struct ConfigSnapshot {
    QString cursorTheme;
    qlonglong cursorSize;
    QString font;
    QString monoFont;
    qlonglong fontSize;
    qlonglong windowRadius;
    QString iconTheme;
    QString activeColor;
    qlonglong windowOpacity;
    qlonglong windowThemeType;
    qlonglong windowTitlebarHeight;
};

ConfigSnapshot g_configSnapshot;
bool g_configSnapshotValid = false;
}

void protocol_test_setup(Helper *helper)
{
    auto *manager = find_server_interface<PersonalizationManagerInterfaceV1>(helper);
    Q_ASSERT(manager);
    QObject::connect(manager, &PersonalizationManagerInterfaceV1::windowContextCreated,
                     [](PersonalizationWindowContextV1 *context) { g_windowContext = context; });
}

extern "C" void personalization_window_state(void *data)
{
    auto *state = static_cast<struct window_context_state *>(data);
    std::memset(state, 0, sizeof(*state));
    if (!g_windowContext)
        return;

    state->background_type = g_windowContext->backgroundType();
    state->corner_radius = g_windowContext->cornerRadius();

    const Shadow shadow = g_windowContext->shadow();
    state->shadow_radius = shadow.radius;
    state->shadow_offset_x = shadow.offset.x();
    state->shadow_offset_y = shadow.offset.y();
    state->shadow_red = shadow.color.red();
    state->shadow_green = shadow.color.green();
    state->shadow_blue = shadow.color.blue();
    state->shadow_alpha = shadow.color.alpha();

    const Border border = g_windowContext->border();
    state->border_width = border.width;
    state->border_red = border.color.red();
    state->border_green = border.color.green();
    state->border_blue = border.color.blue();
    state->border_alpha = border.color.alpha();

    state->no_titlebar =
        g_windowContext->states().testFlag(PersonalizationWindowContextV1::NoTitleBar) ? 1 : 0;
}

extern "C" void personalization_snapshot_config(void *data)
{
    auto *valid = static_cast<int *>(data);
    *valid = 0;
    auto *config = Helper::instance()->config();
    if (!config)
        return;

    g_configSnapshot = {
        .cursorTheme = config->cursorThemeName(),
        .cursorSize = config->cursorSize(),
        .font = config->font(),
        .monoFont = config->monoFont(),
        .fontSize = config->fontSize(),
        .windowRadius = config->windowRadius(),
        .iconTheme = config->iconThemeName(),
        .activeColor = config->activeColor(),
        .windowOpacity = config->windowOpacity(),
        .windowThemeType = config->windowThemeType(),
        .windowTitlebarHeight = config->windowTitlebarHeight(),
    };
    g_configSnapshotValid = true;
    *valid = 1;
}

extern "C" void personalization_restore_config(void *)
{
    if (!g_configSnapshotValid)
        return;

    auto *config = Helper::instance()->config();
    if (!config)
        return;

    config->setCursorThemeName(g_configSnapshot.cursorTheme);
    config->setCursorSize(g_configSnapshot.cursorSize);
    config->setFont(g_configSnapshot.font);
    config->setMonoFont(g_configSnapshot.monoFont);
    config->setFontSize(g_configSnapshot.fontSize);
    config->setWindowRadius(g_configSnapshot.windowRadius);
    config->setIconThemeName(g_configSnapshot.iconTheme);
    config->setActiveColor(g_configSnapshot.activeColor);
    config->setWindowOpacity(g_configSnapshot.windowOpacity);
    config->setWindowThemeType(g_configSnapshot.windowThemeType);
    config->setWindowTitlebarHeight(g_configSnapshot.windowTitlebarHeight);
    Helper::syncPaletteTypeWithWindowThemeType(static_cast<int32_t>(g_configSnapshot.windowThemeType));
    g_configSnapshotValid = false;
}
