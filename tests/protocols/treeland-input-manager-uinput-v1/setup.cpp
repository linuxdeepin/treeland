// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#include "server-bridge.h"
#include "seat/helper.h"
#include "treeland-input-manager-uinput-v1.h"

#include <wbackend.h>
#include <winputdevice.h>

#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>

namespace {
constexpr char kDeviceName[] = "Treeland protocol uinput keyboard";

int g_uinputFd = -1;
bool g_created = false;
bool g_keyboardAdded = false;
bool g_keyboardRemoved = false;
bool g_skip = false;

bool isTestKeyboard(WInputDevice *device)
{
    return device && device->type() == WInputDevice::Type::Keyboard
        && device->name() == QLatin1String(kDeviceName);
}

bool createKeyboard()
{
    g_uinputFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (g_uinputFd < 0)
        return false;

    if (ioctl(g_uinputFd, UI_SET_EVBIT, EV_KEY) < 0
        || ioctl(g_uinputFd, UI_SET_KEYBIT, KEY_A) < 0) {
        close(g_uinputFd);
        g_uinputFd = -1;
        return false;
    }

    uinput_setup setup {};
    std::strncpy(setup.name, kDeviceName, sizeof(setup.name) - 1);
    setup.id.bustype = BUS_VIRTUAL;
    setup.id.vendor = 0xdee1;
    setup.id.product = 0x0001;
    setup.id.version = 1;
    if (ioctl(g_uinputFd, UI_DEV_SETUP, &setup) < 0
        || ioctl(g_uinputFd, UI_DEV_CREATE) < 0) {
        close(g_uinputFd);
        g_uinputFd = -1;
        return false;
    }

    g_created = true;
    return true;
}

void destroyKeyboard()
{
    if (g_uinputFd < 0)
        return;
    ioctl(g_uinputFd, UI_DEV_DESTROY);
    close(g_uinputFd);
    g_uinputFd = -1;
}
}

extern "C" bool protocol_test_preflight()
{
    return access("/dev/uinput", W_OK) == 0;
}

void protocol_test_setup(Helper *helper)
{
    add_headless_output(helper->backend(), false);

    QObject::connect(helper->backend(), &WBackend::inputAdded, helper, [](WInputDevice *device) {
        if (isTestKeyboard(device))
            g_keyboardAdded = true;
    });
    QObject::connect(helper->backend(), &WBackend::inputRemoved, helper, [](WInputDevice *device) {
        if (isTestKeyboard(device))
            g_keyboardRemoved = true;
    });

    if (!createKeyboard())
        g_skip = true;
}

extern "C" bool protocol_test_skip()
{
    return g_skip;
}

extern "C" void input_manager_uinput_read_state(void *data)
{
    *static_cast<input_manager_uinput_state *>(data) = {
        .created = g_created,
        .keyboard_added = g_keyboardAdded,
        .keyboard_removed = g_keyboardRemoved,
    };
}

extern "C" void input_manager_uinput_destroy(void *)
{
    destroyKeyboard();
}
