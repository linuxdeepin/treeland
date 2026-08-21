# `treeland-input-manager-unstable-v1` 测试规范

## 范围

- XML / interface：`treeland_input_manager_v1` / `treeland_pointer_device_configuration_v1` /
  `treeland_keyboard_settings_v1`
- 测试源码：
  - 默认：`tests/protocols/treeland-input-manager-unstable-v1/`
  - 可选系统集成：`tests/protocols/treeland-input-manager-uinput-v1/`
- Fixture：desktop integration fixture（`protocol_test_setup` 创建 headless output）
- 覆盖等级：**I / E（可选）**（默认测试不依赖输入设备；uinput 测试为真实后端 E）

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| 空设备 manager | 绑定 manager + `wl_seat` 并完成 roundtrip | manager 可绑定且无设备 capability 不会导致协议错误或崩溃 |
| 真实设备加入（可选） | `/dev/uinput` 创建虚拟键盘，不发送任何按键 | 真实 libinput 设备触发 `WBackend::inputAdded`，client 收到 Keyboard `capability_available` |
| 真实设备移除（可选） | 销毁同一 uinput 设备 | 真实 libinput 设备触发 `WBackend::inputRemoved`，client 收到 Keyboard `capability_unavailable` |

## 已证明的生产链路

默认 desktop 测试先创建 headless output，再绑定 `wl_seat` 和 manager 并完成 roundtrip。
它不注入 capability，也不伪造 `WInputDevice` 或 wlroots/libinput 设备，因此在无输入设备、
无 `/dev/uinput` 权限的开发和 CI 环境中始终执行，不会以 Skip 掩盖失败。

可选 uinput 测试由 CMake 开关
`TREELAND_ENABLE_UINPUT_PROTOCOL_TESTS=ON` 注册。它将测试 backend 设为
`headless,libinput`，打开 `/dev/uinput` 创建 `BUS_VIRTUAL` 键盘，但绝不写入
`EV_KEY` 输入事件。设备经 udev/libinput 被 wlroots 发现后，`WBackend::inputAdded` 触发
input-manager 的生产 `onInputAdded`；销毁设备则经 `inputRemoved` 触发
`onInputRemoved`。客户端分别断言 Keyboard capability 的 available/unavailable 事件。
此测试没有以 timeout 猜测设备状态：backend 信号及对应 Wayland 事件是唯一验收依据。

服务端实现位于 `src/modules/input-manager/inputmanagerinterfacev1.h`、
`inputmanagerinterfacev1.cpp`。

`bind_resource` 及 capability 广播路径会跳过找不到 `wlr_seat_client` 的 client，故 client
先后绑定 `wl_seat` 与 manager 均不会解引用空 seat client。

## 可选 E 级测试的启用与跳过

uinput 测试默认不加入构建，避免普通开发和 CI 环境访问宿主输入子系统。手动启用：

```bash
cmake --preset default -DTREELAND_ENABLE_UINPUT_PROTOCOL_TESTS=ON
cmake --build --preset default --target test_treeland_input_manager_uinput_v1
ctest --test-dir build --output-on-failure -R '^test_treeland_input_manager_uinput_v1$'
```

没有 `/dev/uinput` 或当前用户无写权限时，测试以退出码 77 标记为 **Skipped**；这不是
通过结果。测试需要可建立 libinput session 的 Linux 环境，若 backend 未接收到设备的
加入或移除信号，断言会立即失败，而不是等待超时。

该层验证的是真实键盘 capability 生命周期，不等同于 mouse/touchpad 的 libinput 配置持久
化；后者仍需要受控的实际指针设备或更深的输入配置系统集成。settings/apply 的设备可用
路径也应放在同类可选 E2E 测试中，而不应通过生产测试钩子伪造 capability。

## 已知边界 / 下一项结果

- mouse/touchpad 对真实 libinput 设备的 settings/apply、配置持久化与生效尚未验证。
- uinput 键盘的 repeat、NumLock 配置写入：当前 E 层的目标仅为热插拔 capability，不把
  虚拟设备配置当作用户输入或物理硬件行为。
- `pointer_device_configuration` 的无设备错误路径：未验证。
