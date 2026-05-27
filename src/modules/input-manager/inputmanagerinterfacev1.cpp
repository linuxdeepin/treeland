// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "inputmanagerinterfacev1.h"
#include "qwayland-server-treeland-input-manager-unstable-v1.h"
#include "helper.h"
#include "common/treelandlogging.h"

#include <qwdisplay.h>
#include <qwinputdevice.h>
#include <qwseat.h>

#include <wbackend.h>
#include <winputdevice.h>

#include <wayland-server-core.h>

static QList<MouseSettingsInterfaceV1 *> s_mouseSettings;
static QList<TouchpadSettingsInterfaceV1 *> s_touchpadSettings;
static QList<KeyboardSettingsInterfaceV1 *> s_keyboardSettings;

class TreelandInputManagerInterfaceV1Private : public QtWaylandServer::treeland_input_manager_v1
{
public:
    explicit TreelandInputManagerInterfaceV1Private(TreelandInputManagerInterfaceV1 *_q);
    wl_global *global() const;

    TreelandInputManagerInterfaceV1 *q = nullptr;
protected:
    void destroy(Resource *resource) override;
    void bind_resource(Resource *resource) override;
    void get_mouse_settings(Resource *resource, uint32_t id, struct ::wl_resource *seat) override;
    void get_touchpad_settings(Resource *resource, uint32_t id, struct ::wl_resource *seat) override;
    void get_keyboard_settings(Resource *resource, uint32_t id, struct ::wl_resource *seat) override;
};

TreelandInputManagerInterfaceV1Private::TreelandInputManagerInterfaceV1Private(TreelandInputManagerInterfaceV1 *_q)
    : QtWaylandServer::treeland_input_manager_v1()
    , q(_q)
{
}

wl_global *TreelandInputManagerInterfaceV1Private::global() const
{
    return m_global;
}

void TreelandInputManagerInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void TreelandInputManagerInterfaceV1Private::bind_resource(Resource *resource)
{
    TreelandInputManagerInterfaceV1::DeviceTypes types = q->inputDeviceListTypes();
    struct wlr_seat_client *seatClient = Helper::instance()->seat()->handle()->client_for_wl_client(resource->client());
    struct wl_resource *clientResource;
    wl_resource_for_each(clientResource, &seatClient->resources) {
        send_capability_available(resource->handle, types.toInt(), clientResource);
    }
}

void TreelandInputManagerInterfaceV1Private::get_mouse_settings(Resource *resource, uint32_t id, struct ::wl_resource *seat)
{
    if (!seat) {
        wl_resource_post_error(resource->handle, 0, "seat resource is NULL!");
        return;
    }

    if (!q->inputDeviceListTypes().testAnyFlag(TreelandInputManagerInterfaceV1::DeviceType::Mouse)) {
        wl_resource_post_error(resource->handle, 0, "No mouse!");
        return;
    }

    wl_resource *mouseResource = wl_resource_create(resource->client(),
                                                    &treeland_mouse_settings_v1_interface,
                                                    resource->version(),
                                                    id);
    if (!mouseResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto mouse = new MouseSettingsInterfaceV1(mouseResource, seat);
    s_mouseSettings.append(mouse);

    QObject::connect(mouse, &QObject::destroyed, [mouse]() {
        s_mouseSettings.removeOne(mouse);
    });

    Q_EMIT q->mouseSettingsCreated(mouse);
}

void TreelandInputManagerInterfaceV1Private::get_touchpad_settings(Resource *resource, uint32_t id, struct ::wl_resource *seat)
{
    if (!seat) {
        wl_resource_post_error(resource->handle, 0, "seat resource is NULL!");
        return;
    }

    if (!q->inputDeviceListTypes().testAnyFlag(TreelandInputManagerInterfaceV1::DeviceType::TouchPad)) {
        wl_resource_post_error(resource->handle, 0, "No TouchPad!");
        return;
    }

    wl_resource *touchpadResource = wl_resource_create(resource->client(),
                                                       &treeland_touchpad_settings_v1_interface,
                                                       resource->version(),
                                                       id);
    if (!touchpadResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto touchpad = new TouchpadSettingsInterfaceV1(touchpadResource, seat);
    s_touchpadSettings.append(touchpad);

    QObject::connect(touchpad, &QObject::destroyed, [touchpad]() {
        s_touchpadSettings.removeOne(touchpad);
    });

    Q_EMIT q->touchpadSettingsCreated(touchpad);
}

void TreelandInputManagerInterfaceV1Private::get_keyboard_settings(Resource *resource, uint32_t id, struct ::wl_resource *seat)
{
    if (!seat) {
        wl_resource_post_error(resource->handle, 0, "seat resource is NULL!");
        return;
    }

    if (!q->inputDeviceListTypes().testFlag(TreelandInputManagerInterfaceV1::DeviceType::Keyboard)) {
        wl_resource_post_error(resource->handle, 0, "No Keyboard!");
        return;
    }

    wl_resource *keyboardResource = wl_resource_create(resource->client(),
                                                       &treeland_keyboard_settings_v1_interface,
                                                       resource->version(),
                                                       id);
    if (!keyboardResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto keyboard = new KeyboardSettingsInterfaceV1(keyboardResource, seat);
    s_keyboardSettings.append(keyboard);

    QObject::connect(keyboard, &QObject::destroyed, [keyboard]() {
        s_keyboardSettings.removeOne(keyboard);
    });

    Q_EMIT q->keyboardSettingsCreated(keyboard);
}

TreelandInputManagerInterfaceV1::TreelandInputManagerInterfaceV1(QObject *parent)
    : QObject(parent)
    , d(new TreelandInputManagerInterfaceV1Private(this))
{
    auto backend = Helper::instance()->backend();
    connect(backend,
            &WBackend::inputAdded,
            this,
            &TreelandInputManagerInterfaceV1::onInputAdded);
    connect(backend,
            &WBackend::inputRemoved,
            this,
            &TreelandInputManagerInterfaceV1::onInputRemoved);
}

void TreelandInputManagerInterfaceV1::sendCapabilityAvailable(TreelandInputManagerInterfaceV1::DeviceTypes types)
{
    for (const auto &resource : d->resourceMap()) {
        struct wlr_seat_client *seatClient =
            Helper::instance()->seat()->handle()->client_for_wl_client(resource->client());
        struct wl_resource *clientResource;
        wl_resource_for_each(clientResource, &seatClient->resources) {
            d->send_capability_available(resource->handle, types.toInt(), clientResource);
        }
    }
}

void TreelandInputManagerInterfaceV1::sendCapabilityUnavailable(TreelandInputManagerInterfaceV1::DeviceTypes types)
{
    for (const auto &resource : d->resourceMap()) {
        struct wlr_seat_client *seatClient =
            Helper::instance()->seat()->handle()->client_for_wl_client(resource->client());
        struct wl_resource *clientResource;
        wl_resource_for_each(clientResource, &seatClient->resources) {
            d->send_capability_unavailable(resource->handle, types.toInt(), clientResource);
        }
    }
}

TreelandInputManagerInterfaceV1::~TreelandInputManagerInterfaceV1() = default;

void TreelandInputManagerInterfaceV1::create(WServer *server)
{
    d->init(server->handle()->handle(), InterfaceVersion);
}

void TreelandInputManagerInterfaceV1::destroy([[maybe_unused]] WServer *server)
{
    d->globalRemove();
}

wl_global *TreelandInputManagerInterfaceV1::global() const
{
    return d->global();
}

TreelandInputManagerInterfaceV1::DeviceTypes TreelandInputManagerInterfaceV1::inputDeviceListTypes() const
{
    TreelandInputManagerInterfaceV1::DeviceTypes types;
    const auto inputDevices = Helper::instance()->backend()->inputDeviceList();
    for (WInputDevice *device : std::as_const(inputDevices)) {
        types |= inputDeviceType(device);
    }

    return types;
}

TreelandInputManagerInterfaceV1::DeviceTypes TreelandInputManagerInterfaceV1::inputDeviceType(WInputDevice *input) const
{
    if (!input->handle()->is_libinput()) {
        return TreelandInputManagerInterfaceV1::DeviceType::Unknown;
    }

    if (input->type() == WInputDevice::Type::Keyboard) {
        return TreelandInputManagerInterfaceV1::DeviceType::Keyboard;
    }

    TreelandInputManagerInterfaceV1::DeviceTypes types;
    struct udev_device *device =
        libinput_device_get_udev_device(wlr_libinput_get_device_handle(input->handle()->handle()));
    if (udev_device_get_property_value(device, "ID_INPUT_MOUSE")) {
        types |= TreelandInputManagerInterfaceV1::DeviceType::Mouse;
    }

    if (udev_device_get_property_value(device, "ID_INPUT_TOUCHPAD")) {
        types |= TreelandInputManagerInterfaceV1::DeviceType::TouchPad;
        types |= TreelandInputManagerInterfaceV1::DeviceType::Mouse;
    }

    return types;
}

QByteArrayView TreelandInputManagerInterfaceV1::interfaceName() const
{
    return d->interfaceName();
}

void TreelandInputManagerInterfaceV1::onInputAdded(WInputDevice *input)
{
    TreelandInputManagerInterfaceV1::DeviceTypes types;
    const auto inputDevices = Helper::instance()->backend()->inputDeviceList();
    for (WInputDevice *device : std::as_const(inputDevices)) {
        if (device == input) {
            continue;
        }

        types |= inputDeviceType(device);
    }

    TreelandInputManagerInterfaceV1::DeviceTypes type = inputDeviceType(input);
    if (!types.testAnyFlags(type)) {
        for (const auto &resource : d->resourceMap()) {
            struct wlr_seat_client *seatClient =
                Helper::instance()->seat()->handle()->client_for_wl_client(resource->client());
            struct wl_resource *clientResource;
            wl_resource_for_each(clientResource, &seatClient->resources) {
                d->send_capability_available(resource->handle, type.toInt(), clientResource);
            }
        }
    }
}

void TreelandInputManagerInterfaceV1::onInputRemoved(WInputDevice *input)
{
    TreelandInputManagerInterfaceV1::DeviceTypes types;
    const auto inputDevices = Helper::instance()->backend()->inputDeviceList();
    for (WInputDevice *device : std::as_const(inputDevices)) {
        if (device == input) {
            continue;
        }

        types |= inputDeviceType(device);
    }

    TreelandInputManagerInterfaceV1::DeviceTypes type = inputDeviceType(input);
    if (!types.testAnyFlags(type)) {
        for (const auto &resource : d->resourceMap()) {
            struct wlr_seat_client *seatClient =
                Helper::instance()->seat()->handle()->client_for_wl_client(resource->client());
            struct wl_resource *clientResource;
            wl_resource_for_each(clientResource, &seatClient->resources) {
                d->send_capability_unavailable(resource->handle, type.toInt(), clientResource);
            }
        }
    }
}

class PointerDeviceConfigurationV1Private : public QtWaylandServer::treeland_pointer_device_configuration_v1
{
public:
    PointerDeviceConfigurationV1Private(PointerDeviceConfigurationV1 *_q,
                                        wl_resource *resource,
                                        wl_resource *_seat);

    PointerDeviceConfigurationV1 *q = nullptr;
    wl_resource *seat = nullptr;

    PointerDeviceConfigurationV1::ChangeFlags pendingChanges =
        PointerDeviceConfigurationV1::NoChange;

    PointerDeviceConfigurationV1::FeatureFlags features = PointerDeviceConfigurationV1::NoFeature;
    double scrollFactor = 0;
    PointerDeviceConfigurationV1::HandedMode handedMode = PointerDeviceConfigurationV1::Right;
    double accelSpeed = 0;
    PointerDeviceConfigurationV1::AccelerationProfile accelerationProfile = PointerDeviceConfigurationV1::None;
    PointerDeviceConfigurationV1::SendEventsModes sendEventsMode = PointerDeviceConfigurationV1::Enabled;
    bool naturalScroll = false;
    bool disableWhileTyping = false;
    bool tapToClick = false;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_scroll_factor(Resource *resource, wl_fixed_t factor) override;
    void set_handed_mode(Resource *resource, uint32_t mode) override;
    void set_accel_speed(Resource *resource, wl_fixed_t accel_speed) override;
    void set_acceleration_profile(Resource *resource, uint32_t profile) override;
    void set_send_events_mode(Resource *resource, uint32_t mode) override;
    void set_natural_scroll(Resource *resource, uint32_t state) override;
    void set_disable_while_typing(Resource *resource, uint32_t state) override;
    void set_tap_to_click(Resource *resource, uint32_t state) override;
    void apply(Resource *resource) override;
};

PointerDeviceConfigurationV1Private::PointerDeviceConfigurationV1Private(PointerDeviceConfigurationV1 *_q,
                                                                         wl_resource *resource,
                                                                         wl_resource *_seat)
    : QtWaylandServer::treeland_pointer_device_configuration_v1(resource)
    , q(_q)
    , seat(_seat)
{
}

void PointerDeviceConfigurationV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void PointerDeviceConfigurationV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void PointerDeviceConfigurationV1Private::set_scroll_factor([[maybe_unused]] Resource *resource,
                                                             wl_fixed_t factor)
{
    scrollFactor = wl_fixed_to_double(factor);
    pendingChanges |= PointerDeviceConfigurationV1::ScrollFactorChanged;
}

void PointerDeviceConfigurationV1Private::set_handed_mode([[maybe_unused]] Resource *resource,
                                                           uint32_t mode)
{
    switch (mode) {
    case TREELAND_POINTER_DEVICE_CONFIGURATION_V1_HANDED_MODE_RIGHT:
        handedMode = PointerDeviceConfigurationV1::Right;
        break;
    case TREELAND_POINTER_DEVICE_CONFIGURATION_V1_HANDED_MODE_LEFT:
        handedMode = PointerDeviceConfigurationV1::Left;
        break;
    default:
        qCCritical(treelandInputManager, "unknown mode in set_handed_mode request");
        return;
    }
    pendingChanges |= PointerDeviceConfigurationV1::HandedModeChanged;
}

void PointerDeviceConfigurationV1Private::set_accel_speed([[maybe_unused]] Resource *resource,
                                                           wl_fixed_t speed)
{
    accelSpeed = wl_fixed_to_double(speed);
    pendingChanges |= PointerDeviceConfigurationV1::AccelSpeedChanged;
}

void PointerDeviceConfigurationV1Private::set_acceleration_profile([[maybe_unused]] Resource *resource,
                                                                    uint32_t profile)
{
    if (!treeland_pointer_device_configuration_v1_acceleration_profile_is_valid(profile, resource->version())) {
        qCCritical(treelandInputManager, "unknown acceleration profile");
        return;
    }

    accelerationProfile = PointerDeviceConfigurationV1::AccelerationProfile(profile);
    pendingChanges |= PointerDeviceConfigurationV1::AccelerationProfileChanged;
}

void PointerDeviceConfigurationV1Private::set_send_events_mode([[maybe_unused]] Resource *resource,
                                                                uint32_t mode)
{
    if (!treeland_pointer_device_configuration_v1_send_events_mode_is_valid(mode, resource->version())) {
        qCCritical(treelandInputManager, "unknown send_events_mode");
        return;
    }

    sendEventsMode = PointerDeviceConfigurationV1::SendEventsModes(mode);
    pendingChanges |= PointerDeviceConfigurationV1::SendEventsModeChanged;
}

void PointerDeviceConfigurationV1Private::set_natural_scroll([[maybe_unused]] Resource *resource,
                                                              uint32_t state)
{
    switch (state) {
    case TREELAND_INPUT_MANAGER_V1_STATE_ENABLED:
        naturalScroll = true;
        break;
    case TREELAND_INPUT_MANAGER_V1_STATE_DISABLED:
        naturalScroll = false;
        break;
    default:
        qCCritical(treelandInputManager, "unknown state in set_natural_scroll request");
        return;
    }
    pendingChanges |= PointerDeviceConfigurationV1::NaturalScrollChanged;
}

void PointerDeviceConfigurationV1Private::set_disable_while_typing([[maybe_unused]] Resource *resource,
                                                                    uint32_t state)
{
    switch (state) {
    case TREELAND_INPUT_MANAGER_V1_STATE_ENABLED:
        disableWhileTyping = true;
        break;
    case TREELAND_INPUT_MANAGER_V1_STATE_DISABLED:
        disableWhileTyping = false;
        break;
    default:
        qCCritical(treelandInputManager, "unknown state in set_disable_while_typing request");
        return;
    }
    pendingChanges |= PointerDeviceConfigurationV1::DisableWhileTypingChanged;
}

void PointerDeviceConfigurationV1Private::set_tap_to_click([[maybe_unused]] Resource *resource,
                                                            uint32_t state)
{
    switch (state) {
    case TREELAND_INPUT_MANAGER_V1_STATE_ENABLED:
        tapToClick = true;
        break;
    case TREELAND_INPUT_MANAGER_V1_STATE_DISABLED:
        tapToClick = false;
        break;
    default:
        qCCritical(treelandInputManager, "unknown state in set_tap_to_click request");
        return;
    }
    pendingChanges |= PointerDeviceConfigurationV1::TapToClickChanged;
}

void PointerDeviceConfigurationV1Private::apply([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->applied(pendingChanges);
    pendingChanges = PointerDeviceConfigurationV1::NoChange;
}

PointerDeviceConfigurationV1::PointerDeviceConfigurationV1(wl_resource *resource,
                                                           wl_resource *seat,
                                                           QObject *parent)
    : QObject(parent)
    , d(new PointerDeviceConfigurationV1Private(this, resource, seat))
{
}

PointerDeviceConfigurationV1::~PointerDeviceConfigurationV1() = default;

void PointerDeviceConfigurationV1::sendFeature(FeatureFlags features, bool force)
{
    if (!force && d->features == features) {
        return;
    }

    d->send_feature(features.toInt());
    d->features = features;
}

void PointerDeviceConfigurationV1::sendScrollFactor(double factor, bool force)
{
    if (!force && d->scrollFactor == factor) {
        return;
    }

    d->send_scroll_factor(wl_fixed_from_double(factor));
    d->scrollFactor = factor;
}

void PointerDeviceConfigurationV1::sendHandedMode(HandedMode mode, bool force)
{
    if (!force && d->handedMode == mode) {
        return;
    }

    d->send_handed_mode(mode);
    d->handedMode = mode;
}

void PointerDeviceConfigurationV1::sendAccelSpeed(double speed, bool force)
{
    if (!force && d->accelSpeed == speed) {
        return;
    }

    d->send_accel_speed(wl_fixed_from_double(speed));
    d->accelSpeed = speed;
}

void PointerDeviceConfigurationV1::sendAccelerationProfile(AccelerationProfile profile, bool force)
{
    if (!force && d->accelerationProfile == profile) {
        return;
    }

    d->send_acceleration_profile(profile);
    d->accelerationProfile = profile;
}

void PointerDeviceConfigurationV1::sendSendEventsMode(SendEventsModes mode, bool force)
{
    if (!force && d->sendEventsMode == mode) {
        return;
    }

    d->send_send_events_mode(mode);
    d->sendEventsMode = mode;
}

void PointerDeviceConfigurationV1::sendNaturalScroll(bool enable, bool force)
{
    if (!force && d->naturalScroll == enable) {
        return;
    }

    d->send_natural_scroll(enable ?
                               TREELAND_INPUT_MANAGER_V1_STATE_ENABLED :
                               TREELAND_INPUT_MANAGER_V1_STATE_DISABLED);
    d->naturalScroll = enable;
}

void PointerDeviceConfigurationV1::sendDisableWhileTyping(bool enable, bool force)
{
    if (!force && d->disableWhileTyping == enable) {
        return;
    }

    d->send_disable_while_typing(enable ?
                                      TREELAND_INPUT_MANAGER_V1_STATE_ENABLED :
                                      TREELAND_INPUT_MANAGER_V1_STATE_DISABLED);
    d->disableWhileTyping = enable;
}

void PointerDeviceConfigurationV1::sendTapToClick(bool enable, bool force)
{
    if (!force && d->tapToClick == enable) {
        return;
    }

    d->send_tap_to_click(enable ?
                             TREELAND_INPUT_MANAGER_V1_STATE_ENABLED :
                             TREELAND_INPUT_MANAGER_V1_STATE_DISABLED);
    d->tapToClick = enable;
}

void PointerDeviceConfigurationV1::sendFailed()
{
    d->send_failed();
}

void PointerDeviceConfigurationV1::sendDone(uint32_t serial)
{
    d->send_done(serial);
}

double PointerDeviceConfigurationV1::scrollFactor() const
{
    return d->scrollFactor;
}

PointerDeviceConfigurationV1::HandedMode PointerDeviceConfigurationV1::handedMode() const
{
    return d->handedMode;
}

double PointerDeviceConfigurationV1::accelSpeed() const
{
    return d->accelSpeed;
}

PointerDeviceConfigurationV1::AccelerationProfile PointerDeviceConfigurationV1::accelerationProfile() const
{
    return d->accelerationProfile;
}

PointerDeviceConfigurationV1::SendEventsModes PointerDeviceConfigurationV1::sendEventsMode() const
{
    return d->sendEventsMode;
}

bool PointerDeviceConfigurationV1::naturalScroll() const
{
    return d->naturalScroll;
}

bool PointerDeviceConfigurationV1::disableWhileTyping() const
{
    return d->disableWhileTyping;
}

bool PointerDeviceConfigurationV1::tapToClick() const
{
    return d->tapToClick;
}

wl_resource *PointerDeviceConfigurationV1::seat() const
{
    return d->seat;
}

WSeat *PointerDeviceConfigurationV1::wSeat() const
{
    struct wlr_seat_client *seat_client =
        wlr_seat_client_from_resource(seat());
    Q_ASSERT_X(seat_client, __func__, "PointerDeviceConfigurationV1 get wlr_seat_client failed.");
    return WSeat::fromHandle(qw_seat::from(seat_client->seat));
}

class MouseSettingsInterfaceV1Private : public QtWaylandServer::treeland_mouse_settings_v1
{
public:
    MouseSettingsInterfaceV1Private(MouseSettingsInterfaceV1 *_q,
                                    wl_resource *_resource,
                                    wl_resource *_seat);

    MouseSettingsInterfaceV1 *q = nullptr;
    wl_resource *seat = nullptr;
    PointerDeviceConfigurationV1 *activeConfig = nullptr;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void get_pointer_configuration(Resource *resource, uint32_t id, uint32_t serial) override;
};

MouseSettingsInterfaceV1Private::MouseSettingsInterfaceV1Private(MouseSettingsInterfaceV1 *_q,
                                                                 wl_resource *resource,
                                                                 wl_resource *_seat)
    : QtWaylandServer::treeland_mouse_settings_v1(resource)
    , q(_q)
    , seat(_seat)
{
}

void MouseSettingsInterfaceV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void MouseSettingsInterfaceV1Private::destroy(Resource *resource)
{
    if (activeConfig) {
        wl_resource_post_error(resource->handle, 0,
                               "destroying treeland_mouse_settings_v1 while pointer_device_configuration_v1 still exists");
    }

    wl_resource_destroy(resource->handle);
}

void MouseSettingsInterfaceV1Private::get_pointer_configuration(Resource *resource,
                                                                uint32_t id,
                                                                [[maybe_unused]] uint32_t serial)
{
    if (activeConfig) {
        wl_resource_post_error(resource->handle, 0,
                               "a treeland_pointer_device_configuration_v1 already exists for this treeland_mouse_settings_v1");
        return;
    }

    wl_resource *configResource = wl_resource_create(resource->client(),
                                                     &treeland_pointer_device_configuration_v1_interface,
                                                     resource->version(),
                                                     id);
    if (!configResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *config = new PointerDeviceConfigurationV1(configResource, seat);
    activeConfig = config;

    QObject::connect(config, &QObject::destroyed, q, [this]() {
        activeConfig = nullptr;
    });

    Q_EMIT q->pointerDeviceConfigurationCreated(config);
}

MouseSettingsInterfaceV1::MouseSettingsInterfaceV1(wl_resource *resource,
                                                   wl_resource *seat,
                                                   QObject *parent)
    : QObject(parent)
    , d(new MouseSettingsInterfaceV1Private(this, resource, seat))
{
}

MouseSettingsInterfaceV1::~MouseSettingsInterfaceV1() = default;

wl_resource *MouseSettingsInterfaceV1::seat() const
{
    return d->seat;
}

WSeat *MouseSettingsInterfaceV1::wSeat() const
{
    struct wlr_seat_client *seat_client =
        wlr_seat_client_from_resource(seat());
    Q_ASSERT_X(seat_client, __func__, "MouseSettingsInterfaceV1 get wlr_seat_client failed.");
    return WSeat::fromHandle(qw_seat::from(seat_client->seat));
}

class TouchpadSettingsInterfaceV1Private : public QtWaylandServer::treeland_touchpad_settings_v1
{
public:
    TouchpadSettingsInterfaceV1Private(TouchpadSettingsInterfaceV1 *_q,
                                       wl_resource *resource,
                                       wl_resource *_seat);

    TouchpadSettingsInterfaceV1 *q = nullptr;
    wl_resource *seat = nullptr;
    PointerDeviceConfigurationV1 *activeConfig = nullptr;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void get_pointer_configuration(Resource *resource, uint32_t id, uint32_t serial) override;
};

TouchpadSettingsInterfaceV1Private::TouchpadSettingsInterfaceV1Private(TouchpadSettingsInterfaceV1 *_q,
                                                                       wl_resource *resource,
                                                                       wl_resource *_seat)
    : QtWaylandServer::treeland_touchpad_settings_v1(resource)
    , q(_q)
    , seat(_seat)
{
}

void TouchpadSettingsInterfaceV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void TouchpadSettingsInterfaceV1Private::destroy(Resource *resource)
{
    if (activeConfig) {
        wl_resource_post_error(resource->handle, 0,
                               "destroying treeland_touchpad_settings_v1 while pointer_device_configuration_v1 still exists");
    }

    wl_resource_destroy(resource->handle);
}

void TouchpadSettingsInterfaceV1Private::get_pointer_configuration(Resource *resource,
                                                                   uint32_t id,
                                                                   [[maybe_unused]] uint32_t serial)
{
    if (activeConfig) {
        wl_resource_post_error(resource->handle, 0,
                               "a treeland_pointer_device_configuration_v1 already exists for this treeland_touchpad_settings_v1");
        return;
    }

    wl_resource *configResource = wl_resource_create(resource->client(),
                                                     &treeland_pointer_device_configuration_v1_interface,
                                                     resource->version(),
                                                     id);
    if (!configResource) {
        wl_client_post_no_memory(resource->client());
        return;
    }

    auto *config = new PointerDeviceConfigurationV1(configResource, seat);
    activeConfig = config;

    QObject::connect(config, &QObject::destroyed, q, [this]() {
        activeConfig = nullptr;
    });

    Q_EMIT q->pointerDeviceConfigurationCreated(config);
}

TouchpadSettingsInterfaceV1::TouchpadSettingsInterfaceV1(wl_resource *resource,
                                                         wl_resource *seat,
                                                         QObject *parent)
    : QObject(parent)
    , d(new TouchpadSettingsInterfaceV1Private(this, resource, seat))
{
}

TouchpadSettingsInterfaceV1::~TouchpadSettingsInterfaceV1() = default;

wl_resource *TouchpadSettingsInterfaceV1::seat() const
{
    return d->seat;
}

WSeat *TouchpadSettingsInterfaceV1::wSeat() const
{
    struct wlr_seat_client *seat_client =
        wlr_seat_client_from_resource(seat());
    Q_ASSERT_X(seat_client, __func__, "TouchpadSettingsInterfaceV1 get wlr_seat_client failed.");
    return WSeat::fromHandle(qw_seat::from(seat_client->seat));
}

class KeyboardSettingsInterfaceV1Private : public QtWaylandServer::treeland_keyboard_settings_v1
{
public:
    KeyboardSettingsInterfaceV1Private(KeyboardSettingsInterfaceV1 *_q,
                                       wl_resource *resource,
                                       wl_resource *_seat);

    KeyboardSettingsInterfaceV1 *q = nullptr;
    wl_resource *seat = nullptr;

    KeyboardSettingsInterfaceV1::ChangeFlags pendingChanges =
        KeyboardSettingsInterfaceV1::NoChange;

    KeyboardSettingsInterfaceV1::FeatureFlags features = KeyboardSettingsInterfaceV1::NoFeature;
    int32_t repeatRate = 0;
    int32_t repeatDelay = 0;
    bool numLock = false;

protected:
    void destroy_resource(Resource *resource) override;
    void destroy(Resource *resource) override;
    void set_repeat(Resource *resource, int32_t rate, int32_t delay) override;
    void set_num_lock(Resource *resource, uint32_t state) override;
    void apply(Resource *resource) override;
};

KeyboardSettingsInterfaceV1Private::KeyboardSettingsInterfaceV1Private(KeyboardSettingsInterfaceV1 *_q,
                                                                       wl_resource *resource,
                                                                       wl_resource *_seat)
    : QtWaylandServer::treeland_keyboard_settings_v1(resource)
    , q(_q)
    , seat(_seat)
{
}

void KeyboardSettingsInterfaceV1Private::destroy_resource([[maybe_unused]] Resource *resource)
{
    delete q;
}

void KeyboardSettingsInterfaceV1Private::destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void KeyboardSettingsInterfaceV1Private::set_repeat([[maybe_unused]] Resource *resource,
                                                    int32_t rate,
                                                    int32_t delay)
{
    repeatRate = rate;
    repeatDelay = delay;
    pendingChanges |= KeyboardSettingsInterfaceV1::RepeatChanged;
}

void KeyboardSettingsInterfaceV1Private::set_num_lock([[maybe_unused]] Resource *resource,
                                                      uint32_t state)
{
    switch (state) {
    case TREELAND_KEYBOARD_SETTINGS_V1_TOGGLE_STATE_ON:
        numLock = true;
        break;
    case TREELAND_KEYBOARD_SETTINGS_V1_TOGGLE_STATE_OFF:
        numLock = false;
        break;
    default:
        qCCritical(treelandInputManager, "unknown state in set_num_lock request");
        return;
    }
    pendingChanges |= KeyboardSettingsInterfaceV1::NumLockChanged;
}

void KeyboardSettingsInterfaceV1Private::apply([[maybe_unused]] Resource *resource)
{
    Q_EMIT q->applied(pendingChanges);
    pendingChanges = KeyboardSettingsInterfaceV1::NoChange;
}

KeyboardSettingsInterfaceV1::KeyboardSettingsInterfaceV1(wl_resource *resource,
                                                         wl_resource *seat,
                                                         QObject *parent)
    : QObject(parent)
    , d(new KeyboardSettingsInterfaceV1Private(this, resource, seat))
{
}

KeyboardSettingsInterfaceV1::~KeyboardSettingsInterfaceV1() = default;

void KeyboardSettingsInterfaceV1::sendFeature(FeatureFlags features, bool force)
{
    if (!force && d->features == features) {
        return;
    }

    d->send_feature(features.toInt());
    d->features = features;
}

void KeyboardSettingsInterfaceV1::sendRepeat(int32_t rate, int32_t delay, bool force)
{
    if (!force && d->repeatRate == rate && d->repeatDelay == delay) {
        return;
    }

    d->send_repeat(rate, delay);
    d->repeatRate = rate;
    d->repeatDelay = delay;
}

void KeyboardSettingsInterfaceV1::sendNumLock(bool enabled, bool force)
{
    if (!force && d->numLock == enabled) {
        return;
    }

    d->send_num_lock(enabled ?
                         TREELAND_KEYBOARD_SETTINGS_V1_TOGGLE_STATE_ON :
                         TREELAND_KEYBOARD_SETTINGS_V1_TOGGLE_STATE_OFF);
    d->numLock = enabled;
}

void KeyboardSettingsInterfaceV1::sendFailed()
{
    d->send_failed();
}

void KeyboardSettingsInterfaceV1::sendDone()
{
    d->send_done();
}

int32_t KeyboardSettingsInterfaceV1::repeatRate() const
{
    return d->repeatRate;
}

int32_t KeyboardSettingsInterfaceV1::repeatDelay() const
{
    return d->repeatDelay;
}

bool KeyboardSettingsInterfaceV1::numLock() const
{
    return d->numLock;
}

wl_resource *KeyboardSettingsInterfaceV1::seat() const
{
    return d->seat;
}

WSeat *KeyboardSettingsInterfaceV1::wSeat() const
{
    struct wlr_seat_client *seat_client =
        wlr_seat_client_from_resource(seat());
    Q_ASSERT_X(seat_client, __func__, "KeyboardSettingsInterfaceV1 get wlr_seat_client failed.");
    return WSeat::fromHandle(qw_seat::from(seat_client->seat));
}
