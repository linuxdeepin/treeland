# input-manager uinput v1

此 target 仅在 `TREELAND_ENABLE_UINPUT_PROTOCOL_TESTS=ON` 时注册。它创建一个
`BUS_VIRTUAL` uinput 键盘，验证真实 `uinput → libinput → WBackend` 热插拔路径使
input-manager 向 Wayland client 发送 Keyboard capability 的 available/unavailable 事件。

CTest 使用 `WLR_BACKENDS=headless,libinput`，并显式采用 Treeland 服务的
`LIBSEAT_BACKEND=seatd` 和 `SEATD_SOCK=/run/dde-seatd.sock`。启动前，runner 会同时检查
`/dev/uinput` 写权限和对 CTest 注入的 `SEATD_SOCK` 的实际连接；缺少任一条件以退出码 77
跳过，不能视为通过。

尚未覆盖物理按键事件、pointer/touch 设备，以及 input-manager settings/apply 的行为。
