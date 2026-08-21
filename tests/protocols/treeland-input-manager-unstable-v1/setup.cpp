// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "server-bridge.h"
#include "modules/input-manager/inputmanagerinterfacev1.h"
#include "seat/helper.h"
#include "treeland-input-manager-unstable-v1.h"

#include <wbackend.h>
#include <wserver.h>

namespace {
input_mgr_state g_state;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    auto *manager = helper->backend()->server()->findInterface<TreelandInputManagerInterfaceV1>();
    Q_ASSERT(manager);

    QObject::connect(manager,
                     &TreelandInputManagerInterfaceV1::touchpadSettingsCreated,
                     helper,
                     [helper](TouchpadSettingsInterfaceV1 *settings) {
                         QObject::connect(settings,
                                          &TouchpadSettingsInterfaceV1::pointerDeviceConfigurationCreated,
                                          helper,
                                          [helper](PointerDeviceConfigurationV1 *config) {
                                              QObject::connect(config,
                                                               &PointerDeviceConfigurationV1::applied,
                                                               helper,
                                                               [config](PointerDeviceConfigurationV1::ChangeFlags changes) {
                                                                   g_state.pointer_changes = changes.toInt();
                                                                   g_state.pointer_scroll_factor = config->scrollFactor();
                                                                   g_state.pointer_handed_mode = static_cast<int>(config->handedMode());
                                                                   g_state.pointer_accel_speed = config->accelSpeed();
                                                                   g_state.pointer_accel_profile = static_cast<uint32_t>(config->accelerationProfile());
                                                                   g_state.pointer_send_events_mode = config->sendEventsMode().toInt();
                                                                   g_state.pointer_natural_scroll = config->naturalScroll();
                                                                   g_state.pointer_disable_while_typing = config->disableWhileTyping();
                                                                   g_state.pointer_tap_to_click = config->tapToClick();
                                                               });
                                          });
                     });
    QObject::connect(manager,
                     &TreelandInputManagerInterfaceV1::keyboardSettingsCreated,
                     helper,
                     [helper](KeyboardSettingsInterfaceV1 *settings) {
                         QObject::connect(settings,
                                          &KeyboardSettingsInterfaceV1::applied,
                                          helper,
                                          [settings](KeyboardSettingsInterfaceV1::ChangeFlags changes) {
                                              g_state.keyboard_changes = changes.toInt();
                                              g_state.keyboard_repeat_rate = settings->repeatRate();
                                              g_state.keyboard_repeat_delay = settings->repeatDelay();
                                              g_state.keyboard_num_lock = settings->numLock();
                                          });
                     });
}

extern "C" void input_manager_read_state(void *data)
{
    *static_cast<input_mgr_state *>(data) = g_state;
}
